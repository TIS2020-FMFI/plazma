import time
import subprocess
import threading
import queue
import testing
# import os
# from datetime import datetime


class Adapter:
    MAX_RESPONSE_TIME = 10.0  # ako dlho caka na pristroj aby odpovedal, v sekundach
    MAX_HPCTRL_RESPONSE_TIME = 10  # ako dlho caka na program HPCTRL aby vyprintoval dalsi riadok, v sekundach

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
        # self.in_queue = None

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
                # out.close()
                return
            if line:
                self.out_queue.put(line)

            else:
                time.sleep(0.001)
            print("enqueue" + line)
            # nekonecny cyklus, thread vzdy cita z pipe a hadze do out_queue po riadkoch
        # out.close()
        # time.sleep(1)

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

    def get_output(self):
        out_str = ''        
        get_started = time.time()
        counter = 0

        while (time.time() < get_started + self.MAX_HPCTRL_RESPONSE_TIME):
            if self.out_queue.empty():
                time.sleep(0.01)
                counter += 1
                if (counter > 300) and (out_str != ""):
                    return out_str.strip()
            else:
                out_str += self.out_queue.get_nowait()
                counter = 0
                
        print("read timeout")
        return None
        
    def hpctrl_is_responsive(self):
        if not self.send("ping"):
            return False

        out = self.get_output()
        if out is None:
            self.restart_hpctrl()  # restartujem ho, lebo nereaguje
            return False

        if out.strip() == "!unknown command ping":
            return True
        # out = out.split(' ')
        # if out[0] == "!unknown" and out[1] == "command":
        #     return True

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
            time.sleep(0.8)  # aby HPCTRL stihol spracovat prikaz, inak vypisuje !not ready, try again later (ping)
            return True
        except OSError:
            print("Padol HPCTRL")
            self.restart_hpctrl()
            return False

    def kill_hpctrl(self):
        if self.process is not None:
            self.send("exit")
            time.sleep(0.5)
        if self.out_thread is not None:
            self.out_thread_killed = True
            self.out_thread.join()
            print("out_thread joined")
            self.out_thread = None
        if self.process is not None:
            self.process.kill()
            # self.process.terminate()
            self.process = None
        self.out_queue = None

    def start_hpctrl(self):
        # TODO napisat do GUI spatnu vazbu ak je zla cesta...
        # try:
        self.process = subprocess.Popen(["hpctrl-main/src/Debug/hpctrl.exe", "-i"], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        # except:
        #     pass

        self.out_queue = queue.Queue()

        self.out_thread = threading.Thread(target=self.enqueue_output)
        self.out_thread.daemon = True
        self.out_thread.start()

    def restart_hpctrl(self):
        # TODO NEFUNGUJE po 4h debugovania :(
        print("RESTARTUJEM HPCTRL")
        self.kill_hpctrl()
        time.sleep(5)
        self.start_hpctrl()

    def connect(self, address):
        if self.testing:
            if self.test.connect(address):
                self.address = address
                self.connected = True
                return True
            else:
                return False

        if self.send("CONNECT " + str(address)):
            if self.hpctrl_is_responsive():
                self.address = address
                self.connected = True
                return True

        print("Error s HPCTRL pri connektuvani")
        return False

    def disconnect(self):
        if self.testing:
            self.test.disconnect()
            self.connected = False
            self.address = None  # potrebujem adresu vobec si pametat?
            return

        self.send("DISCONNECT")
        # disconnect prikaz nevypisuje nic ak uz bol

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
                return self.get_output()
            else:
                return None
        return False

    def set_state(self, state):  # state = string
        # TODO ked sa nieco zle posle...
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
            if self.send("SETSTATE\n" + state):  # neviem ci takto pojde alebo musim dat dalsi send()?
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

    # def get_calibration_type(self):
    #     # TODO zistit ako co
    #     pass

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
                return self.get_output()
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
            if self.send("SETCALIB\n" + calibration):  # neviem ci takto pojde alebo musim dat dalsi send()?
                return True
            else:
                return None
        return False

    def set_port1_length(self, value):
        pass

    def set_port2_length(self, value):
        pass

    def set_velocity_factor(self, value):
        pass

    def set_frequency_unit(self):  # asi nebude v adaptery? bar ni v tomto tvare
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
            if self.send("CMD\ns STAR " + str(value) + " " + unit + "\n.\n"):  # este overit ci mozem takto z CMD ist von
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
            if self.send("CMD\ns STOP " + str(value) + " " + unit + "\n.\n"):  # este overit ci mozem takto z CMD ist von
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
            if self.send("CMD\ns POIN " + str(value) + "\n.\n"):  # este overit ci mozem takto z CMD ist von
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
                self.test.format = p_format  # nikdy sa negetuje asi
                return True
            else:
                return False

        if self.connected:
            if self.send("FMT " + p_format):
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

        if self.connected:
            for param in parameters.split():
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

        functions = [self.set_frequency_unit,
                     self.set_start_frequency,
                     self.set_stop_frequency,
                     self.set_points,
                     self.set_data_format,
                     self.set_parameters]

        for f in functions:
            result = f()
            if not result:  # false alebo none
                return result
        return True

    # self.set_frequency_unit()
    # self.set_start_frequency()
    # self.set_stop_frequency()
    # self.set_points()
    # self.set_data_format()
    # self.set_parameters()

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
                return self.get_output()
            else:
                return None
        return False

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

        return self.get_output()

    def enter_cmd_mode(self):
        # if self.testing:
        #     if self.connected:
        #         if not self.in_cmd_mode:
        #             print("Terminal mode ON")
        #             self.in_cmd_mode = True
        #         return True
        #     else:
        #         return False

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
        # if self.testing:
        #     if self.connected and self.in_cmd_mode:
        #         print("Terminal mode OFF")
        #         self.in_cmd_mode = False
        #         return True
        #     # else:
        #     #     return False

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
                        return self.get_output()
                    return True
                else:
                    return None
            print("not in cmd je False")
            return False
        return False

# open_terminal()
# close_terminal()
# send_command(command)
# ping()
# restart_connection()
# time.sleep() pouzivat vsade po kazdom prikaze alebo nejako inak?
