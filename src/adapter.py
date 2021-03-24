import time
import subprocess
import threading
import queue
import testing
import tkinter as tk


class Adapter:

    CALIBRATION_TIMEOUT = 12    # seconds

    def __init__(self, program):
        self.testing = True
        #self.testing = False
        if self.testing:
            self.test = testing.Test()

        self.program = program
        self.address = None
        self.connected = False
        self.in_cmd_mode = False

        self.out_queue = None
        self.process = None
        self.out_thread = None
        self.out_thread_killed = False
        self.start_hpctrl()

    def enqueue_output(self):
        out = self.process.stdout
        for line in iter(out.readline, b''):
            if self.out_thread_killed:
                out.close()
                return
            if line:
                self.out_queue.put(line)
            else:
                time.sleep(0.001)
            # nekonecny cyklus, thread vzdy cita z pipe a hadze do out_queue po riadkoch

    def get_output(self, timeout, lines=None):
        out_str = ''
        get_started = time.time()
        line_counter = 0
        while time.time() < get_started + timeout:
            if lines is not None and line_counter >= lines:
                return out_str.strip()
            if self.out_queue.empty():
                time.sleep(0.001)
            else:
                out_str += self.out_queue.get_nowait()
                line_counter += 1

        out_str = out_str.strip()
        if out_str:
            return out_str
        return None

    def clear_input_queue(self):
        while not self.out_queue.empty():
            self.out_queue.get_nowait()

    def start_hpctrl(self):
        path = "hpctrl.exe"
        try:
            self.process = subprocess.Popen([path, "-i"], stdin=subprocess.PIPE,
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        except FileNotFoundError:
            tk.messagebox.showerror(title="HPCTRL error", message="Wrong HPCTRL path!\n"
                                                                  f"Expected to find HPCTRL here: \"./{path}\"")
            quit()

        self.out_queue = queue.Queue()

        self.out_thread = threading.Thread(target=self.enqueue_output)
        self.out_thread.daemon = True
        self.out_thread.start()
        self.out_thread_killed = False

    def kill_hpctrl(self):
        if self.process is not None:
            self.send("exit")
        if self.out_thread is not None:
            self.out_thread_killed = True
            self.out_thread.join()
            self.out_thread = None
        if self.process is not None:
            self.process.terminate()
            self.process.kill()
            self.process = None
        self.out_queue = None

    def restart_hpctrl(self):
        self.kill_hpctrl()  # moze zase zavolat restart
        self.start_hpctrl()
        
    def hpctrl_is_responsive(self):
        self.clear_input_queue()
        if not self.send("ping"):
            return False
        out = self.get_output(4, 1)        
        if out is None:            
            self.restart_hpctrl()  # restartujem ho, lebo nereaguje
            return False
        if out.strip() == "!unknown command ping":
            return True
        self.restart_hpctrl()
        return False

    def send(self, message):
        try:
            print(message, file=self.process.stdin)
            self.process.stdin.flush()
            # TODO toto bolo pri hpctrl zmenene tak otestovat ci funguje bez sleep
            time.sleep(0.1)  # aby HPCTRL stihol spracovat prikaz, inak vypisuje !not ready, try again later (ping)
            return True
        except OSError:
            if message.strip().lower() != "exit":
                self.restart_hpctrl()
            return False

    def connect(self, address):
        if self.testing:
            if self.test.connect(address):
                self.address = address
                self.connected = True
                return True
            else:
                return False

        if self.send("LOGON\nCONNECT " + str(address)):
            if self.hpctrl_is_responsive():
                if self.send("CMD\n"
                             "s PORE ON\n"
                             "."):
                    self.address = address
                    self.connected = True
                    return True
        return False

    def disconnect(self):
        if self.testing:
            self.test.disconnect()
            self.connected = False
            self.address = None
            return

        self.send("DISCONNECT")
        self.address = None
        self.connected = False

    def get_state(self):
        if self.testing:
            if self.connected:
                time.sleep(2)  # iba kvoli testovaniu ci sa GUI zamrzne
                return self.test.get_state()
            else:
                return False

        if self.connected:
            if self.send("GETSTATE"):
                output = self.get_output(4, 1)
                if output is None:
                    return None
                output += "\n" + self.get_output(1)
                return output
            else:
                return None
        return False

    def set_state(self, state):  # state = string
        if state is None or state == "":
            return False
        if self.testing:
            if self.connected:
                self.test.set_state("------Toto je stav, ktory bol poslaty do pristroja: "
                                    + self.program.project.get_state())
                return True
            else:
                return False

        if self.connected:
            if self.send("SETSTATE\n" + state):
                return True
            else:
                return None
        return False

    def preset(self):
        if self.testing:
            if self.connected:
                self.test.reset()
                return True
            else:
                return False

        if self.connected:
            if self.send("FACTRESET"):
                return True
            else:
                return None
        return False

    def get_calibration(self):
        if self.testing:
            if self.connected:
                time.sleep(2)  # iba kvoli testovaniu ci sa GUI zamrzne
                return self.test.get_calib()
            else:
                return

        if self.connected:
            if self.send("GETCALIB"):
                output = self.get_output(10, 1)  # 5s ci staci na poslanie aj 12 kaliracii?
                if output is None:
                    return None
                if output == "":  # ak je nenakalibrovany
                    return output
                get_calib_started = time.time()
                while True:
                    new_line = self.get_output(0.5, 1)
                    if new_line == "":
                        break
                    if new_line is None:
                        if time.time() > get_calib_started + self.CALIBRATION_TIMEOUT:
                            return None
                        else:
                            continue
                    output += "\n" + new_line
                return output
            else:
                return None
        return False

    def set_calibration(self, calibration):
        if self.testing:
            if self.connected:
                self.test.set_calib("----Zmenena kalibracia: " + self.program.project.get_calibration())
                return True
            else:
                return False

        if self.connected:
            if self.send("SETCALIB\n" + calibration):
                return True
            else:
                return None
        return False

    def set_port1_length(self):
        value = self.program.settings.get_port1()
        value *= 0.000000000001
        if self.testing:
            return True

        if self.connected:
            if self.enter_cmd_mode():
                if self.send(f"s PORT1 {value}"):
                    if self.exit_cmd_mode():
                        return True
                    else:
                        return None
                else:
                    return None
            else:
                return None
        return False

    def set_port2_length(self):
        value = self.program.settings.get_port2()
        value *= 0.000000000001
        if self.testing:
            return True

        if self.connected:
            if self.enter_cmd_mode():
                if self.send(f"s PORT2 {value}"):
                    if self.exit_cmd_mode():
                        return True
                    else:
                        return None
                else:
                    return None
            else:
                return None
        return False

    def set_velocity_factor(self):
        value = self.program.settings.get_vel_factor()
        if self.testing:
            return True

        if self.connected:
            if self.enter_cmd_mode():
                if self.send(f"s VELOFACT {value}"):
                    if self.exit_cmd_mode():
                        return True
                    else:
                        return None
                else:
                    return None
            else:
                return None
        return False

    def set_frequency_unit(self):
        unit = self.program.settings.get_freq_unit().strip()
        if unit.upper() not in ("GHZ", "MHZ"):
            return False
        if self.testing:
            if self.connected:
                self.test.set_freq_unit(unit)
                return True
            else:
                return False

        if self.connected:
            if self.send("FREQ " + unit + "\n"):
                return True
            else:
                return None
        return False

    def set_start_frequency(self):
        unit = self.program.settings.get_freq_unit().strip()
        value = self.program.settings.get_freq_start()
        if self.testing:
            if self.connected:
                if unit.upper() == "GHZ":
                    value *= 1.0e+09
                elif unit.upper() == "MHZ":
                    value *= 1.0e+06
                else:
                    return False
                self.test.set_min_freq(value)
                return True
            else:
                return False

        if self.connected:
            if self.send("s STAR " + str(value) + " " + unit + "\n"):
                return True
            else:
                return None
        return False

    def set_stop_frequency(self):
        unit = self.program.settings.get_freq_unit().strip()
        value = self.program.settings.get_freq_stop()
        if self.testing:
            if self.connected:
                if unit.upper() == "GHZ":
                    value *= 1.0e+09
                elif unit.upper() == "MHZ":
                    value *= 1.0e+06
                else:
                    return False
                self.test.set_max_freq(value)
                return True
            else:
                return False

        if self.connected:
            if self.send("s STOP " + str(value) + " " + unit + "\n"):
                return True
            else:
                return None
        return False

    def set_points(self):
        value = self.program.settings.get_points()
        if self.testing:
            if self.connected:
                self.test.set_points(value)
                return True
            else:
                return False

        if self.connected:
            if self.send("s POIN " + str(value) + "\n"):
                # return True
                if self.send("q POIN?"):
                    new_value = self.get_output(4, 1)
                    if new_value is not None:
                        new_value = int(float(new_value))
                        self.program.settings.set_points(new_value)
                        return True
                    return None
                else:
                    return None
            else:
                return None
        return False

    def set_data_format(self):  # format = string RI/MA/DB
        # nastavi format v hpctrl, nie v pristroji, nevypisuje 'not connected'
        p_format = self.program.settings.get_parameter_format().strip().upper()
        if p_format not in ("RI", "MA", "DB"):
            return False

        if self.testing:
            if self.connected:
                self.test.format = p_format
                return True
            else:
                return False

        if self.connected:
            if self.send("FMT " + p_format + "\n"):
                return True
            else:
                return None
        return False

    def set_parameters(self):  # parameters = string S11 .. S22
        # nastavi parametre v hpctrl, nie v pristroji, nevypisuje 'not connected'
        parameters = self.program.settings.get_parameters().strip().upper()
        for param in parameters.split():
            if param not in ("S11", "S12", "S21", "S22"):
                return

        if self.testing:
            if self.connected:
                self.test.params = parameters  # nikdy sa negetuje asi
                return True
            else:
                return False

        parameters = parameters.split()
        parameters.insert(0, "CLEAR")
        if self.connected:
            for param in parameters:
                if not self.send(param):
                    return None
            return True
        return False

    def prepare_measurement(self):
        if self.testing:
            self.test.set_freq_unit(self.program.settings.get_freq_unit())
            self.test.set_min_freq(self.program.settings.get_freq_start())
            self.test.set_max_freq(self.program.settings.get_freq_stop())
            self.test.set_points(self.program.settings.get_points())
            self.test.params = self.program.settings.get_parameters()
            self.test.format = self.program.settings.get_parameter_format()

        address = self.address
        self.disconnect()
        return_code = self.connect(address)
        if not return_code:
            return return_code
        time.sleep(1.7)
        functions = [self.set_data_format,
                     self.set_parameters,
                     self.set_frequency_unit,
                     self.enter_cmd_mode,
                     self.set_start_frequency,
                     self.set_stop_frequency,
                     self.set_points,
                     self.set_port1_length,
                     self.set_port2_length,
                     self.set_velocity_factor,
                     self.exit_cmd_mode]

        for f in functions:
            return_code = f()
            if not return_code:  # false alebo none
                return return_code
        return True

    def measure(self):
        if self.testing:
            if self.connected:
                return_code = self.prepare_measurement()
                if not return_code:  # false alebo None
                    return return_code
                time.sleep(1)  # iba na kvoli simulacii testovania
                return self.test.get_data()
            else:
                return False

        if self.connected:
            return_code = self.prepare_measurement()
            if not return_code:  # false alebo None
                return return_code

            if self.send("MEASURE"):
                parameters = self.program.settings.get_parameters().strip().split()
                points = self.program.settings.get_points()
                output = ""
                for param in range(len(parameters)):
                    line = self.get_output(20, 1)    # z testovacich merani sa 1601 points vymeria cca za 3.5s
                    # ale 4.2.2021 pri teste nam s kratsim casom(10s) to neslo, takze zatial 20s !
                    if line is None:
                        return ""
                    output += line + "\n"

                riadok = self.get_output(3, 1)  # staci? neviem ci sa hned posielaju data
                while True:
                    if riadok is None:
                        return False
                    output += riadok + "\n"
                    if len(riadok) > 0 and riadok[0] == "#":
                        break
                    riadok = self.get_output(1, 1)

                data = self.get_output(10, points)
                if data is None:
                    return False
                output += data
                if not self.out_queue.empty():
                    riadok = self.get_output(1, 1)
                    if riadok.strip() == "":
                        return output
                return output
            else:
                return None
        return False

    def start_measurement(self):
        if self.testing:
            if self.connected:
                return_code = self.prepare_measurement()
                if not return_code:  # false alebo None
                    return return_code
                return True
            else:
                return False

        if self.connected:
            return_code = self.prepare_measurement()
            if not return_code:  # false alebo None
                return return_code
            if self.send("M+"):
                return True
            else:
                return None
        return False

    def end_measurement(self):
        if self.testing:
            if self.connected:
                self.test.data_order_number = 0
            else:
                return False

        if self.connected:
            if self.send("M-"):
                return True
            else:
                return None
        return False

    def retrieve_measurement_data(self):
        if self.testing:
            time.sleep(1)  # iba kvoli testing
            data = self.test.get_data(more_measurement=True)
            if data is not None:
                self.test.data_order_number += 1
            return data

        parameters = self.program.settings.get_parameters().strip().split()
        points = self.program.settings.get_points()
        output = ""
        for param in range(len(parameters)):
            line = self.get_output(10, 1)  # z testovacich merani sa 1601 points vymeria cca za 3.5s
            # ale 4.2.2021 pri teste nam s kratsim casom(10s) to neslo, takze zatial 20s !
            if line is None:
                return ""
            output += line + "\n"

        riadok = self.get_output(3, 1)  # staci? neviem ci sa hned posielaju data
        while True:
            if riadok is None:
                return None
            output += riadok + "\n"
            if len(riadok) > 0 and riadok[0] == "#":
                break
            riadok = self.get_output(1, 1)

        data = self.get_output(10, points)
        if riadok is None:
            return None
        if not self.out_queue.empty():
            riadok = self.get_output(1, 1)
            if riadok.strip() == "":
                return output
        output += data
        return output

    def enter_cmd_mode(self):
        if self.connected:
            if not self.in_cmd_mode:
                if self.send("CMD\n"):
                    self.in_cmd_mode = True
                    return True
                else:
                    return None
            return True
        return False

    def exit_cmd_mode(self):
        if self.connected:
            if self.in_cmd_mode:
                if self.send("\n.\n"):
                    self.in_cmd_mode = False
                    return True
                else:
                    return None
            return True
        return False

    def cmd_send(self, message):
        if self.connected:
            if self.in_cmd_mode:
                message = message.strip()
                message = message.lower()
                if message in (".", "cmd", "exit"):
                    return True
                index = message.find(" ")
                if index > 0:
                    prve_slovo = message[:message.find(" ")]
                else:
                    prve_slovo = message
                if self.send(f"{message}\n"):
                    self.in_cmd_mode = True
                    prve_slovo = prve_slovo.strip()
                    if prve_slovo in ("q", "a", "c", "d", "b", "?", "help"):
                        output = self.get_output(10, 1)
                        if output is None:
                            return None
                        while not self.out_queue.empty():
                            riadky = self.get_output(0.2)
                            if riadky is not None:
                                output += "\n" + riadky
                        return output
                    return True
                else:
                    return None
            return False
        return False
