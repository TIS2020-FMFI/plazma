from adapter import Adapter
from project import Project
from gui import Gui
from file_manager import FileManager
import threading
import queue
import time
from settings import Settings
from terminal import Terminal
import tkinter as tk


class Program:
    def __init__(self):
        self.settings = Settings()
        self.adapter = Adapter(self)
        self.project = Project(self)
        self.gui = Gui(self)
        self.file_manager = FileManager(self)
        self.terminal = Terminal(self)

        self.function_queue = queue.Queue()
        self.work_thread = threading.Thread(target=self.execute_functions)
        self.work_thread.daemon = True
        self.work_thread.start()
        self.measuring = False
        self.gui_thread_is_free = True

    def execute_functions(self):
        while True:
            try:
                function = "self." + self.function_queue.get()
                exec(function)
            except queue.Empty:
                time.sleep(0.2)

    def queue_function(self, function):
        self.function_queue.put(function)

    # should be executed by self.work_thread only
    def connect(self, address):
        if self.adapter.connect(address):
            self.gui.gpib.update_button_connected()
            self.gui.info.change_connect_label()
        else:
            tk.messagebox.showerror(title="Connection error", message="Can't connect to the device on that address.")

    # should be executed by self.work_thread only
    def disconnect(self):
        if self.terminal.is_open():
            self.close_terminal()
        self.adapter.disconnect()
        self.gui.gpib.update_button_disconnected()
        self.gui.info.change_connect_label()

    # should be executed by self.work_thread only
    def save_state(self):
        state = self.adapter.get_state()
        if state is None:
            print("Error pri save_state()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not state:
            print("Ved ani nie si konektnuty")
        else:
            print("State: \n" + state)
            self.project.set_state(state)
            self.gui.info.change_state_label()

        #  ---- len na testovanie adapter.restart_hpctrl()
        # print('help', file=self.adapter.process.stdin)
        # self.adapter.process.stdin.flush()
        # time.sleep(1)
        #
        # if self.adapter.hpctrl_is_responsive():
        #     print("responsive")
        # else:
        #     print("NOT responsive")

        # self.adapter.send("help")
        #
        # output = self.adapter.get_output()  # moze vratit None
        # print("SKONCIL GET_OUTPUT HELP: " + str(output))
        #
        # print('done')

    # should be executed by self.work_thread only
    def recall_state(self):
        print("Posielas stav: \n" + self.project.get_state())

        return_code = self.adapter.set_state(self.project.get_state())
        if return_code is None:
            print("Error pri recall_state()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not return_code:
            print("Ved ani nie si konektnuty")
        else:
            tk.messagebox.showinfo(title="Recalled State", message="State sent to the device successfully!")

    # should be executed by self.work_thread only
    def preset(self):
        return_code = self.adapter.preset()
        if return_code is None:
            print("Error pri recall_state()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not return_code:
            print("Ved ani nie si konektnuty")
        else:
            tk.messagebox.showinfo(title="PRESET", message="Preset successful!")

    def save_calib(self):
        calib = self.adapter.get_calibration()
        if calib is None:
            print("Error pri save_calib()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif type(calib) == str and not calib:
            tk.messagebox.showwarning(title="Empty calibration", message="The device is not calibrated.")
        elif not calib:
            print("Ved ani nie si konektnuty")
        else:
            print("Calibration: \n" + calib)
            self.project.set_calibration(calib)
            self.gui.info.change_calibration_label()

    def load_calib(self):
        print("Posielas kalibraciu: \n" + self.project.get_calibration())

        return_code = self.adapter.set_calibration(self.project.get_calibration())
        if return_code is None:
            print("Error pri load_calib()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not return_code:
            print("Ved ani nie si konektnuty")
        else:
            tk.messagebox.showinfo(title="Calibration Loaded", message="Calibration sent to the device successfully!")

    # should be executed by self.work_thread only
    def prepare_measurement(self):
        parameters = self.settings.get_parameters()
        parameters = parameters.upper().split()
        s11 = s12 = s21 = s22 = False
        if "S11" in parameters:
            s11 = True
        if "S12" in parameters:
            s12 = True
        if "S21" in parameters:
            s21 = True
        if "S22" in parameters:
            s22 = True
        self.project.reset_data(s11, s12, s21, s22)

    # should be executed by self.work_thread only
    def measure(self, autosave=False):
        self.prepare_measurement()
        if self.project.data is None:  # ak neboli vybrate ziadne S-parametre, vymazu sa data z pameti
            self.gui.sweep.change_run()
            self.gui.window.after_idle(self.gui.sweep.refresh_frame)
            return

        data = self.adapter.measure()
        if data is None:
            print("Error pri merani, neprisli ziadne data")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
            self.project.data = None
            return
        if data == "":
            print("Neprisli ziadne data z adapteru, mozno treba restart")  # mozno nie je v Remote pristroj
            tk.messagebox.showwarning(title="No Data", message="No data received.\n"
                                                               "The device may be in Remote mode, try reconnecting.")
            return
        if not data:
            print("Alebo nie si konektnuty, alebo riadky dat nezodpovedaju ocakavanym")
            self.project.data = None
            return

        data = data.strip()
        print("Data ktore prisli: \n" + data)

        self.project.data.add_measurement(data)
        self.gui.window.after_idle(self.gui.sweep.refresh_frame)
        if autosave:
            self.file_manager.save_last_measurement()
        print()
        print("pocet merani: " + str(self.project.data.number_of_measurements))
        print("parametre: " + str(self.project.data.parameters))
        meranie = self.project.data.get_number_of_measurements() - 1
        prva_freq = list(self.project.data.measurements_list[meranie][1])[0]
        posledna_freq = list(self.project.data.measurements_list[meranie][1])[-1]
        print("Data v pameti:")  # + str(self.project.data.measurements_list[meranie]))
        print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")
        print("Start_freq[from]: " + str(prva_freq))
        print("Stop_freq[to]: " + str(posledna_freq))
        print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")
        print()

    # should be executed by self.work_thread only
    def start_measurement(self, autosave=False):
        self.prepare_measurement()
        if self.project.data is None:  # ak neboli vybrate ziadne S-parametre
            self.gui.sweep.reset_frame()
            self.gui.sweep.change_run()
            self.gui.window.after_idle(self.gui.sweep.refresh_frame)
            return

        return_code = self.adapter.start_measurement()
        if return_code is None:
            self.gui.sweep.change_run()
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
            self.project.data = None
            return
        if not return_code:  # ak nebol konektnuty
            self.gui.sweep.change_run()
            self.project.data = None
            return

        self.measuring = True
        print("startujem merania")

        waited = False
        while True:
            data = self.adapter.retrieve_measurement_data()  # moze dlhsie trvat
            if data is None or data == "":
                # self.adapter.end_measurement()
                if waited:
                    # stane sa ak som uz spracoval posledne data od HPCTRL
                    break
                if data == "":
                    print("Neprisli ziadne data z adapteru, mozno treba restart")  # mozno nie je v Remote pristroj
                    tk.messagebox.showwarning(title="No Data", message="No data received.\n"
                                                                       "The device may be in "
                                                                       "Remote mode, try reconnecting.")
                    self.gui.sweep.change_run()
                    return
                print("Error pri merani, neprisli ziadne data")
                self.measuring = False
                self.gui.gpib.update_button_disconnected()
                self.gui.info.change_connect_label()
                self.gui.sweep.change_run()
                return
            print("Data ktore prisli: \n" + data)

            data = data.strip()
            self.project.data.add_measurement(data)

            if self.gui_thread_is_free:  # aby gui reagoval na klikanie pocas merania
                # time.sleep(0.2)
                self.gui.window.after(1, self.toggle_gui_free)
                self.gui.window.after(1, self.gui_graph_refresh)

                # self.gui.window.after(1, self.gui.sweep.refresh_frame)
                # self.gui.window.after(1000, self.toggle_gui_free)

                # self.gui.window.after_idle(self.toggle_gui_free)
                # self.gui.window.after_idle(self.gui.sweep.refresh_frame)
                # self.gui.window.after_idle(self.toggle_gui_free)
                print("THE END GUI ----------------------------------------------------------------------------------")
            else:
                print("----------------------------------------------------------------------------------\n"
                      "----------------------------------------------------------------------------------\n"
                      "----------------------------------------------------------------------------------\n"
                      "----------------------------------------------------------------------------------\n"
                      "----------------------------------------------------------------------------------\n"
                      "----------------------------------------------------------------------------------")
            if autosave:
                self.file_manager.save_last_measurement()
            print()
            print("pocet merani: " + str(self.project.data.number_of_measurements))
            print("parametre: " + str(self.project.data.parameters))
            print("Data v pameti:")  # + str(self.project.data.measurements_list[meranie]))
            print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")

            print("data v pameti: ")
            for i, val in enumerate(self.project.data.measurements_list):
                print(f"    {i+1}. Meranie: ")
                print(f"        hlavicka: {repr(val[0])}")
                print(f"        data: {val[1]}")
            print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")
            print()

            if waited:
                break
            if not self.measuring:   # malo by byt thread-safe
                waited = True

        print("Ukoncene merania - uz necakam na data")
        self.gui.window.after(1, self.gui.sweep.refresh_frame)
        # self.gui.window.after_idle(self.gui.sweep.refresh_frame)
        self.gui.sweep.change_run()

    def toggle_gui_free(self):
        self.gui_thread_is_free = not self.gui_thread_is_free

        # run by main thread
    def gui_graph_refresh(self):
        self.gui.sweep.refresh_frame()
        self.gui.window.after(200, self.toggle_gui_free)

    # executed by GUI thread
    def end_measurement(self):
        def finish():
            print("stopujem merania")
            self.adapter.end_measurement()
            time.sleep(0.2)  # chcem pockat kym HPCTRL vyprintuje meranie do std_out ak uz zacal
            self.measuring = False
            print("Zastavene meranie - mozno este pockam na posledne data ak pridu")

        if not self.measuring:
            print("Ved som ani nic nemeral !!!")
            return

        finish_thread = threading.Thread(target=finish)
        finish_thread.daemon = True
        finish_thread.start()

    def get_data_for_graph(self, measurement_index, s_param):
        measurement = self.project.data.get_measurement(s_param, measurement_index)
        if measurement is None:
            return None
        freq = measurement.keys()
        value1 = []
        value2 = []
        for v1, v2 in measurement.values():
            value1.append(v1)
            value2.append(v2)
        return freq, value1, value2

    def open_terminal(self):
        return_code = self.adapter.enter_cmd_mode()
        if return_code is None:
            print("Error pri enter_terminal()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not return_code:
            print("Ved ani nie si konektnuty")
        else:
            print('Podarilo sa vojst do terminal modu')
            self.terminal.open_new_window()

    def close_terminal(self):
        return_code = self.adapter.exit_cmd_mode()
        if return_code is None:
            print("Error pri close_terminal()")
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        else:
            print('Podarilo sa vinst von z terminal modu')

    def terminal_send(self, message):
        return_code = self.adapter.cmd_send(message)
        if return_code is None:
            print("Error pri terminal_send()\n" + message)
            self.gui.gpib.update_button_disconnected()
            self.gui.info.change_connect_label()
        elif not return_code:
            print("Ved ani nie si konektnuty, alebo si spravne nevosiel do terminalu predtym")
        else:
            print("Podarilo sa poslat spravu: " + str(message))
            if type(return_code) != bool:
                print("Chcem vyprintovat spravu: " + return_code)
                self.terminal.print_message(return_code)
            else:
                print("VRATILO BOOL?")

    # should be executed by self.work_thread only
    def adjust_calibration(self, port1, port2, vel_fact):
        adjust_list = [port1, port2, vel_fact]
        functions = [self.adapter.set_port1_length, self.adapter.set_port2_length, self.adapter.set_velocity_factor]
        for i in range(len(functions)):
            return_code = functions[i](adjust_list[i])
            if return_code is None:
                print("Error pri adjust_calibration()")
                self.gui.gpib.update_button_disconnected()
                self.gui.info.change_connect_label()
            elif not return_code:
                print("Ved ani nie si konektnuty")
        # tk.messagebox.showinfo(title="ADJUST sent", message="Adjust data for calibration sent!")
        self.settings.set_port1(float(port1))
        self.settings.set_port2(float(port2))
        self.settings.set_vel_factor(float(vel_fact))

    def quit_program(self):
        self.gui.window.quit()  # ukonci mainloop, takze __main__ skonci = cely program skonci bez chyby
        self.gui.window.destroy()  # aby to spravne zavrelo window ked sa program spusti cez IDLE
        self.adapter.kill_hpctrl()


if __name__ == '__main__':
    main = Program()
    main.gui.window.protocol("WM_DELETE_WINDOW", main.quit_program)
    main.gui.window.mainloop()
