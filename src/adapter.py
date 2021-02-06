import time
import subprocess
import threading
import queue
import testing
import tkinter as tk


class Adapter:
    # MAX_RESPONSE_TIME = 10.0  # ako dlho caka na pristroj aby odpovedal, v sekundach
    # MAX_HPCTRL_RESPONSE_TIME = 10  # ako dlho caka na program HPCTRL aby vyprintoval dalsi riadok, v sekundach

    # vzdy defaultne vecie ako 0.05, pre pomalsie PC sa da zvacsit ak nieco pada

    def __init__(self, program):
        # self.testing = True
        self.testing = False
        if self.testing:
            self.test = testing.Test()

        self.program = program
        self.address = None
        self.connected = False
        # self.frequency_format = "GHz"
        self.in_cmd_mode = False

        self.out_queue = None
        self.process = None
        self.out_thread = None
        self.out_thread_killed = False
        self.start_hpctrl()

        #####################################################

        # print('help', file=self.process.stdin)
        # self.process.stdin.flush()
        # time.sleep(1)
        #
        # output = self.get_output()
        # print("SKONCIL GET_OUTPUT HELP: " + output)
        #
        # print('done')

        #####################################################

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
            print("enqueue:" + repr(line))
            # nekonecny cyklus, thread vzdy cita z pipe a hadze do out_queue po riadkoch

##    def get_output(self):
##        out_str = ''
##        slept = False
##        counter = 0
##        max_cycles = max(self.MAX_RESPONSE_TIME / max(self.MAX_HPCTRL_RESPONSE_TIME, 0.05), 2)
##
##        while out_str.strip() == "" or slept:
##            print("zacinam")
##            try:
##                while True:
##                    print("kek, out_str: \n" + out_str)	
##                    out_str += self.out_queue.get_nowait()
##                    slept = False
##                    counter = 0
##
##            except queue.Empty:
##                print("empty queue, slept: " + str(slept))
##                if slept and out_str.strip() != "":
##                    return out_str
##
##                wait_started = time.time()
##                while self.out_queue.empty() and (time.time() < wait_started + self.MAX_HPCTRL_RESPONSE_TIME):
##                    pass
##                slept = self.out_queue.empty()
##
##                # time.sleep(self.MAX_HPCTRL_RESPONSE_TIME)
##                # uistenie sa ze na 100% vytiahnem cely vystup, a nie iba cast
##                # slept = True
##
##                counter += 1
##                print(counter)
##                if counter > max_cycles:
##                    print(f"Presiel som {max_cycles} cyklov(minimalne {self.MAX_RESPONSE_TIME}s)"
##                          + " a nedostal som odpoved z pristroja")
##                    return None
##        print("Error pri citani outputu - sem by sa nikdy nemalo dostat")

    # def get_output(self):
    #     out_str = ''
    #     get_started = time.time()
    #     counter = 0
    #
    #     while (time.time() < get_started + self.MAX_HPCTRL_RESPONSE_TIME):
    #         if self.out_queue.empty():
    #             time.sleep(0.01)
    #             counter += 1
    #             if (counter > 300) and (out_str != ""):
    #                 return out_str.strip()
    #         else:
    #             out_str += self.out_queue.get_nowait()
    #             counter = 0
    #
    #     print("read timeout")
    #     return None

    def get_output(self, timeout, lines=None):
        out_str = ''
        get_started = time.time()
        line_counter = 0

        while time.time() < get_started + timeout:
            if lines is not None and line_counter >= lines:
                print("READ ALL LINES----------------------------------------")
                print(repr(out_str.strip()))
                print("LINE END----------------------------------------------")
                return out_str.strip()
            if self.out_queue.empty():
                time.sleep(0.001)
            else:
                out_str += self.out_queue.get_nowait()
                line_counter += 1

        print("READ timeout")
        out_str = out_str.strip()
        if out_str:
            return out_str
        return None

    def start_hpctrl(self):
        print("zaciatok start")
        path = "hpctrl-main/src/Debug/hpctrl.exe"
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
        print("Koniec start")

    def kill_hpctrl(self):
        print("zaciatok kill")
        if self.process is not None:
            self.send("exit")
        if self.out_thread is not None:
            self.out_thread_killed = True
            self.out_thread.join()
            print("out_thread joined")
            self.out_thread = None
        if self.process is not None:
            self.process.terminate()
            self.process.kill()
            self.process = None
        self.out_queue = None
        print("Koniec kill")

    def restart_hpctrl(self):
        print("RESTARTUJEM ")
        self.kill_hpctrl()  # moze zase zavolat restart
        self.start_hpctrl()
        
    def hpctrl_is_responsive(self):
        if not self.send("ping"):
            return False

        out = self.get_output(1, 1)
        if out is None:
            self.restart_hpctrl()  # restartujem ho, lebo nereaguje
            return False

        if out.strip() == "!unknown command ping":
            return True

        print(out.strip())
        print("HPCTRL returned something unexpected, restarting")
        self.restart_hpctrl()
        return False

    def send(self, message):
        try:
            print(message, file=self.process.stdin)
            # if self.process is None:
            #     self.start_hpctrl()
            self.process.stdin.flush()
            print("Poslal som: " + message)
            # TODO toto bolo pri hpctrl zmenene tak otestovat ci funguje bez sleep
            time.sleep(0.1)  # aby HPCTRL stihol spracovat prikaz, inak vypisuje !not ready, try again later (ping)
            return True
        except OSError:
            print("Padol HPCTRL")
            if message.strip().lower() != "exit":
                self.restart_hpctrl()
            return False

    def connect(self, address):
        print("CONNECT")
        if self.testing:
            if self.test.connect(address):
                self.address = address
                self.connected = True
                return True
            else:
                return False

        if self.send("CONNECT " + str(address)):
            if self.hpctrl_is_responsive():
                if self.send("CMD\n"
                             "s PORE ON\n"
                             "."):
                    self.address = address
                    self.connected = True
                    return True

        print("Error s HPCTRL pri connektuvani")
        return False

    def disconnect(self):
        if self.testing:
            self.test.disconnect()
            self.connected = False
            self.address = None
            return

        self.send("DISCONNECT")
        # disconnect prikaz nevypisuje nic ak uz bol disconnectnuty
        self.address = None
        self.connected = False

    def get_state(self):
        if self.testing:
            if self.connected:
                print('GETSTATE = 2s cakanie')
                time.sleep(2)  # iba kvoli testovaniu ci sa GUI zamrzne
                print('GETSTATE skoncil')
                return self.test.get_state()
            else:
                print('Error - Not connected')
                return False

        if self.connected:
            if self.send("GETSTATE"):
                output = self.get_output(4, 1)
                if output is None:
                    return None
                output += "\n" + self.get_output(1)
                if not self.out_queue.empty():
                    print("Pri get_state: Queue nie je empty!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
                    print("Nasl. riadok v queue:" + repr(self.out_queue.get_nowait()))
                return output
            else:
                return None
        return False

    def set_state(self, state):  # state = string
        if state is None or state == "":
            print("Trying to send state, but state is empty")
            return False
        if self.testing:
            if self.connected:
                self.test.set_state("------Toto je stav, ktory bol poslaty do pristroja: "
                                    + self.program.project.get_state())
                return True
            else:
                print('Error - Not connected')
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
                print('Error - Not connected')
                return False

        if self.connected:
            if self.send("RESET"):
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
                print('Error - Not connected')
                return

        if self.connected:
            if self.send("GETCALIB"):
                output = self.get_output(5, 1)  # 5s ci staci na poslanie aj 12 kaliracii?
                if output is None:
                    return None
                if output == "":  # ak je nenakalibrovany
                    return output
                output += "\n" + self.get_output(1)
                if not self.out_queue.empty():
                    print("Pri get_calibration: Queue nie je empty!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
                    print("Nasl. riadok v queue:" + repr(self.out_queue.get_nowait()))
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
                print('Error - Not connected')
                return False

        if self.connected:
            if self.send("SETCALIB\n" + calibration):
                return True
            else:
                return None
        return False

    def set_port1_length(self, value):
        value *= 0.000000000001
        print("Posielam port1: " + str(value))
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

    def set_port2_length(self, value):
        value *= 0.000000000001
        print("Posielam port2: " + str(value))
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

    def set_velocity_factor(self, value):
        print("Posielam vel_fact: " + str(value))
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
            print("Zla jednotka frekvencie !!!")
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
                    print("Zla jednotka frekvencie !!!")
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
                    print("Zla jednotka frekvencie !!!")
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
                return True
            else:
                return None
        return False

    def set_data_format(self):  # format = string RI/MA/DB
        # nastavi format v hpctrl, nie v pristroji, nevypisuje 'not connected'
        p_format = self.program.settings.get_parameter_format().strip().upper()
        if p_format not in ("RI", "MA", "DB"):
            print("Zly format dat !!!")
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
                print("Zle napisane parametre !!!")
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
            return True

        address = self.address
        self.disconnect()
        return_code = self.connect(address)
        if not return_code:
            return return_code

        functions = [self.set_data_format,
                     self.set_parameters,
                     self.set_frequency_unit,
                     self.enter_cmd_mode,
                     self.set_start_frequency,
                     self.set_stop_frequency,
                     self.set_points,
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

                print("Zacinam meranie s hodnotami: ")
                print("-------------------------------")
                print("jednotka freq: " + self.test.freq_unit)
                print("start freq: " + str(self.test.min_freq))
                print("stop freq: " + str(self.test.max_freq))
                print("points: " + str(self.test.points))
                print("parameters: " + str(self.test.params))
                print("format: " + self.test.format)
                print("-------------------------------")
                time.sleep(2)  # iba na kvoli simulacii testovania

                time.sleep(0.1)  # musim pockat aby mi nieco prislo,
                # este aby som si bol isty ze mi hpctrl posle naraz cele meranie
                return self.test.get_data()
            else:
                print('Error - Not connected')
                return False

        if self.connected:
            return_code = self.prepare_measurement()
            if not return_code:  # false alebo None
                return return_code

            if self.send("MEASURE"):
                parameters = self.program.settings.get_parameters().strip().split()
                points = self.program.settings.get_points()
                output = ""
                print("PARAMETERS:")
                print(parameters)
                for param in range(len(parameters)):
                    print("Prechadzam: " + parameters[param])
                    line = self.get_output(20, 1)    # z testovacich merani sa 1601 points vymeria cca za 3.5s
                    # ale 4.2.2021 pri teste nam s kratsim casom(10s) to neslo, takze zatial 20s !
                    if line is None:
                        print("Nepodarilo sa precitat prve riadky pri merani")
                        return ""
                    output += line + "\n"

                print("IDEM NA HLAVICKU")
                riadok = self.get_output(3, 1)  # staci? neviem ci sa hned posielaju data
                while True:
                    if riadok is None:
                        print("Nepodarilo sa precitat hlavicku pri merani")
                        return False
                    output += riadok + "\n"
                    if len(riadok) > 0 and riadok[0] == "#":
                        print("nasiel som #")
                        break
                    riadok = self.get_output(1, 1)

                print("---------------Points:-------------------")
                print(points)
                print("---------------Koniec Points-------------")
                data = self.get_output(10, points)
                if data is None:
                    print("Nepodarilo sa precitat riadky s datami pri merani")
                    return False
                output += data
                if not self.out_queue.empty():
                    riadok = self.get_output(1, 1)
                    if riadok.strip() == "":
                        return output
                    print("QUEUE NIE JE PRAZDNY PO MEASURE() !!!")
                    print("Nasl. riadok v queue:" + repr(riadok))
                return output
            else:
                return None
        return False

        # if self.send("MEASURE"):
        #     return self.get_output()
        # else:
        #     return None
    # return False

    def start_measurement(self):
        if self.testing:
            if self.connected:
                return_code = self.prepare_measurement()
                if not return_code:  # false alebo None
                    return return_code

                print("Zacinam meranie s hodnotami: ")
                print("-------------------------------")
                print("jednotka freq: " + self.test.freq_unit)
                print("start freq: " + str(self.test.min_freq))
                print("stop freq: " + str(self.test.max_freq))
                print("points: " + str(self.test.points))
                print("parameters: " + str(self.test.params))
                print("format: " + self.test.format)
                print("-------------------------------")

                # posles M+
                # zavolas get_output.. cakas.
                return True  # self.test.get_data()
            else:
                print('Error - Not connected')
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
                print("Poslal som M- do HPCTRL")
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
            time.sleep(2)
            data = self.test.get_data(more_measurement=True)
            if data is not None:
                self.test.data_order_number += 1
            return data
        # return self.get_output()

        parameters = self.program.settings.get_parameters().strip().split()
        points = self.program.settings.get_points()
        output = ""
        print("PARAMETERS:")
        print(parameters)
        for param in range(len(parameters)):
            print("Prechadzam: " + parameters[param])
            line = self.get_output(20, 1)  # z testovacich merani sa 1601 points vymeria cca za 3.5s
            # ale 4.2.2021 pri teste nam s kratsim casom(10s) to neslo, takze zatial 20s !
            if line is None:
                print("Nepodarilo sa precitat prve riadky pri merani - continous")
                return ""
            output += line + "\n"

        riadok = self.get_output(3, 1)  # staci? neviem ci sa hned posielaju data
        while True:
            if riadok is None:
                print("Nepodarilo sa precitat hlavicku pri merani - continous")
                return None
            output += riadok + "\n"
            if len(riadok) > 0 and riadok[0] == "#":
                break
            riadok = self.get_output(1, 1)

        data = self.get_output(10, points)
        if riadok is None:
            print("Nepodarilo sa precitat riadky s datami pri merani - continous")
            return None
        if not self.out_queue.empty():
            riadok = self.get_output(1, 1)
            if riadok.strip() == "":
                return output
            print("QUEUE NIE JE PRAZDNY PO retrieve_measurement_data() !!!")
            print("Nasl. riadok v queue:" + repr(riadok))
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
                if self.send("\n.\n"):  # este overit ci mozem takto z CMD ist von
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
                    print("Poslal si zakazany prikaz, ignorujem ho")
                    return True
                index = message.find(" ")
                if index > 0:
                    prve_slovo = message[:message.find(" ")]
                else:
                    prve_slovo = message
                # TODO poriesit ostatne vstupy atd
                if self.send(f"{message}\n"):
                    self.in_cmd_mode = True
                    print("ADAPTER message:" + message)
                    print("ADAPTER message:" + prve_slovo)
                    prve_slovo = prve_slovo.strip()
                    if prve_slovo in ("q", "a", "b", "?", "help"):
                        print("CAKAM NA ODPOVED")
                        # timeout = 5
                        # start_time = time.time()
                        # while True:
                        #     if not self.out_queue.empty():
                        #         break
                        #     if time.time() <= start_time + timeout:
                        #         return None
                        # return self.get_output(2)
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
            print("not in cmd_mode")
            return False
        return False
