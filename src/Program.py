from adapter import Adapter
from project import Project
from gui import Gui
from file_manager import FileManager
import threading
import queue
import time
from settings import Settings


class Program:
    def __init__(self):
        self.adapter = Adapter(self)
        self.project = Project(self)
        self.gui = Gui(self)
        self.file_manager = FileManager(self)
        self.settings = Settings()

        self.function_queue = queue.Queue()
        self.work_thread = threading.Thread(target=self.execute_functions)
        self.work_thread.daemon = True
        self.work_thread.start()
        self.measuring = False

    def execute_functions(self):
        while True:
            try:
                function = "self." + self.function_queue.get()
                exec(function)
            except queue.Empty:
                time.sleep(0.5)

    def queue_function(self, function):
        self.function_queue.put(function)

    # should be executed by self.work_thread only
    def connect(self, address):
        if self.adapter.connect(address):
            self.gui.gpib.update_button_connected()
            self.gui.info.change_connect_label()
        else:
            print('Nepodarilo sa pripojit')

    # should be executed by self.work_thread only
    def disconnect(self):
        self.adapter.disconnect()
        self.gui.gpib.update_button_disconnected()
        self.gui.info.change_connect_label()

    # should be executed by self.work_thread only
    def save_state(self):
        state = self.adapter.get_state()
        if not state and state is not None:
            print("Ved ani nie si konektnuty")
        elif state is None:
            print("Error pri save_state()")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
        else:
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
        if not return_code and return_code is not None:
            print("Ved ani nie si konektnuty")
        elif return_code is None:
            print("Error pri recall_state()")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
        else:
            print('Podarilo sa poslat stav')
            # TODO mozno GUI aby vypisalo nejaku spetnu vezbu?

    # should be executed by self.work_thread only
    def preset(self):
        return_code = self.adapter.preset()
        if not return_code and return_code is not None:
            print("Ved ani nie si konektnuty")
        elif return_code is None:
            print("Error pri recall_state()")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
        else:
            print('Podarilo sa urobit preset()')
            # TODO mozno GUI aby vypisalo nejaku spetnu vezbu?

    def save_calib(self):
        calib = self.adapter.get_calibration()
        if not calib and calib is not None:
            print("Ved ani nie si konektnuty")
        elif calib is None:
            print("Error pri save_calib()")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
        else:
            self.project.set_calibration(calib)
            self.gui.info.change_calibration_label()

    def load_calib(self):
        print("Posielas kalibraciu: \n" + self.project.get_calibration())

        return_code = self.adapter.set_calibration(self.project.get_calibration())
        if not return_code and return_code is not None:
            print("Ved ani nie si konektnuty")
        elif return_code is None:
            print("Error pri load_calib()")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
        else:
            print('Podarilo sa poslat kalibraciu')
            # TODO mozno GUI aby vypisalo nejaku spetnu vezbu?

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
    def measure(self):
        self.prepare_measurement()
        if self.project.data is None:  # ak neboli vybrate ziadne S-parametre, vymazu sa data z pameti
            # TODO prejde do stavu 2
            # TODO refreshni Frame(vymazali sa data)
            return

        data = self.adapter.measure()

        if not data and data is not None:
            print("Ved ani nie si konektnuty")
            self.project.data = None
            return
        elif data is None:
            print("Error pri merani, neprisli ziadne data")
            # TODO prejde do stavu 1(nekonektnuty) lebo sa resetne hpctrl
            self.project.data = None
            return

        # data = self.adapter.retrieve_measurement_data()

        print("Data ktore prisli: \n" + data)

        self.project.data.add_measurement(data)
        # TODO refreshni Frame
        print()
        print("pocet merani: " + str(self.project.data.number_of_measurements))
        print("parametre: " + str(self.project.data.parameters))
        print("data v pameti: " + str(self.project.data.measurements_list))
        print()
        # TODO prejde do stavu 2

    # should be executed by self.work_thread only
    def start_measurement(self):
        self.prepare_measurement()
        if self.project.data is None:  # ak neboli vybrate ziadne S-parametre
            # TODO prejde do stavu 2
            # TODO refreshni Frame(vymazali sa data)
            return

        return_code = self.adapter.start_measurement()
        if not return_code and return_code is not None:  # ak nebol konektnuty
            self.project.data = None
            return
        elif return_code is None:
            # TODO prejde do stavu 1
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
                print("Error pri merani, neprisli ziadne data")
                self.measuring = False
                # TODO prejde do stavu 1
                return
            print("Data ktore prisli: \n" + data)

            self.project.data.add_measurement(data)
            # TODO refreshni Frame
            print()
            print("pocet merani: " + str(self.project.data.number_of_measurements))
            print("parametre: " + str(self.project.data.parameters))
            print("data v pameti: ")
            for i, val in enumerate(self.project.data.measurements_list):
                print(f"    {i+1}. Meranie: ")
                print(f"        hlavicka: {repr(val[0])}")
                print(f"        data: {val[1]}")
            print()

            if waited:
                break
            if not self.measuring:   # malo by byt thread-safe
                waited = True

        print("Ukoncene merania - uz necakam na data")
        # TODO prejde do stavu 2

    # executed by GUI thread
    def end_measurement(self):
        def finish():
            # TODO tu sa zmeni STOP button na RUN
            print("stopujem merania")
            self.adapter.end_measurement()
            time.sleep(0.2)  # chcem pockat kym HPCTRL vyprintuje meranie do std_out ak uz zacal
            self.measuring = False
            print("Zastavene meranie - mozno este pockam na posledne data ak pridu")

        if not self.measuring:
            print("Ved som ani nic nemeral !!!")
            # TODO nemal by sa zmenit STOP button
            return

        finish_thread = threading.Thread(target=finish)
        finish_thread.daemon = True
        finish_thread.start()


if __name__ == '__main__':
    main = Program()
    main.gui.window.mainloop()
