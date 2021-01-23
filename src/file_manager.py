import tkinter
import os
from datetime import datetime
from tkinter import filedialog

class FileManager:
    def __init__(self, program):
        # self.settings = {}  # None    #  Dictionary, dict()
        # self.filepath = None
        self.description = ""
        self.program = program

    def save_project(self, path, project_name, description):
        # TODO savuje aj ked kliknem na Cancel
        filepath = os.path.join(path, project_name)

        now = datetime.now()
        time = datetime.timestamp(now)
        date = datetime.fromtimestamp(time)
        date = str(date)
        date = date.replace(":", "-")
        
        if os.path.exists(filepath):
          filepath += " " + str(date)
          
        os.mkdir(filepath)

        # Description
        name = "description.txt"
        file_path = os.path.join(filepath, name)
        f = open(file_path, "w")
        f.write(description)
        f.close()

        # settings
        settings = self.program.settings.print_settings()
        name = "settings.txt"
        file_path = os.path.join(filepath, name)
        f = open(file_path, "w")
        f.write(settings)
        f.close()

        # State
        state = self.program.project.get_state()
        if state is not None:
            name = "state.txt"
            file_path = os.path.join(filepath, name)
            f = open(file_path, "w")
            f.write(state)
            f.close()

        # Calibration
        calibration = self.program.project.get_calibration()
        if calibration is not None:
            name = "calibration.txt"
            file_path = os.path.join(filepath, name)
            f = open(file_path, "w")
            f.write(calibration)
            f.close()

        # Data
        if self.program.project.data is not None:
            os.mkdir(filepath + "\\measurements")

            for i in range(self.program.project.data.get_number_of_measurements()):
                data = self.program.project.data.print_measurement(i)
                # data_time = self.program.project.data.print_measurement_date()
                # data_time = data_time.replace(":", "-")
                if data is not None:
                    name = "measurements\\measurement" + str(i+1) + ".s2p"  # + data_time + ".s2p"
                    file_path = os.path.join(filepath, name)
                    f = open(file_path, "w")
                    f.write(data)
                    f.close()

        # po ulozeni projektu sa zavola metoda v INFO GUI aby pouzivatel videl ze ulozil
        self.program.gui.info.change_project_label()

    def load_project(self, path):
        filepath = path + "/" + "description.txt"
        with open(filepath) as file:
            for line in file:
                self.description += line
        print(self.description)

        filepath = path + "/" + "settings.txt"
        zoznam = []
        with open(filepath) as f:
            for line in f:
                value = line.split("=")[-1].strip()
                zoznam.append(value)

    # def get_settings(self):
    #     return self.settings
    #
    # def set_settings(self, address, port1, port2, vel_factor, freq_unit, freq_start, freq_stop,
    #                  points, parameter_format, parameters, continuous, autosave):
    #     self.settings["address"] = address
    #     self.settings["port1"] = port1
    #     self.settings["port2"] = port2
    #     self.settings["vel_factor"] = vel_factor
    #     self.settings["freq_unit"] = freq_unit
    #     self.settings["freq_start"] = freq_start
    #     self.settings["freq_stop"] = freq_stop
    #     self.settings["points"] = points
    #     self.settings["parameter_format"] = parameter_format
    #     self.settings["parameters"] = parameters
    #     self.settings["continuous"] = continuous
    #     self.settings["autosave"] = autosave
    #     ...
    #     pass

    # def save_settings(self):
    #     if self.filepath is not None:
    #         print(self.filepath)
    #         file = open(self.filepath + "\\" + "settings.txt", "w")
    #         for key, value in self.settings.items():
    #             file.write(str(key) + " = " + str(value) + "\n")
    #         file.close()
    #     else:
    #         file = open("settings.txt", "w")
    #         for key, value in self.settings.items():
    #             file.write(str(key) + " = " + str(value) + "\n")
    #         file.close()
    #     # vyrobi settings.txt v kazdom riadku  kluc = hodnota
    #     # address = 16
    #     pass
    
