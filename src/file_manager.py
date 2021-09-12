import os
from datetime import datetime
from settings import Settings


class FileManager:
    def __init__(self, program):
        self.description = ""
        self.program = program
        self.autosave_path = ""
        self.autosave_name = ""

    def save_project(self, path, project_name, description, autosave=False):
        if autosave:
            self.autosave_path = path
        filepath = os.path.join(path, project_name)
        if autosave:
            self.autosave_name = project_name

        now = datetime.now()
        time = datetime.timestamp(now)
        date = datetime.fromtimestamp(time)
        date = str(date)
        date = date.replace(":", "-")
        if os.path.exists(filepath):
            filepath += " " + str(date)
            if autosave:
                self.autosave_name += " " + str(date)
          
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
        if not self.program.project.exists_data():
            return
        os.mkdir(filepath + "\\measurements")

        pocet = self.program.project.data.get_number_of_measurements()
        for i in range(pocet):
            data = self.program.project.data.print_measurement(i)
            if data is not None:
                name = "measurements\\measurement" + str(i+1) + ".s2p"  # + data_time + ".s2p"
                file_path = os.path.join(filepath, name)
                f = open(file_path, "w")
                f.write(data)
                f.close()

    def load_project(self, path):
        filepath = path + "/" + "description.txt"
        try:
            self.description = ""
            with open(filepath) as file:
                for line in file:
                    self.description += line
        except FileNotFoundError:
            self.description = None
            pass

        # Settings
        filepath = path + "/" + "settings.txt"
        try:
            zoznam = []
            with open(filepath) as f:
                for line in f:
                    value = line.split("=")[-1].strip()
                    zoznam.append(value)
            self.program.settings.set_address(int(zoznam[0]))
            self.program.settings.set_port1(float(zoznam[1]))
            self.program.settings.set_port2(float(zoznam[2]))
            self.program.settings.set_vel_factor(float(zoznam[3]))
            self.program.settings.set_freq_unit(zoznam[4])
            self.program.settings.set_freq_start(float(zoznam[5]))
            self.program.settings.set_freq_stop(float(zoznam[6]))
            self.program.settings.set_points(int(zoznam[7]))
            self.program.settings.set_parameters(zoznam[8])
            self.program.settings.set_parameter_format(zoznam[9])
            if zoznam[10] == "True":
                self.program.settings.set_continuous(True)
            else:
                self.program.settings.set_continuous(False)
        except FileNotFoundError:
            self.program.settings = Settings()
        self.program.gui.gpib.load_project_settings()

        # State
        filepath = path + "/" + "state.txt"
        try:
            state = ""
            with open(filepath) as f:
                for line in f:
                    state += line
            self.program.project.set_state(state)
        except FileNotFoundError:
            self.program.project.reset_state()
            self.program.gui.info.change_state_label()

        # Calibration
        filepath = path + "/" + "calibration.txt"
        try:
            calibration = ""
            with open(filepath) as f:
                for line in f:
                    calibration += line
            self.program.project.set_calibration(calibration)
        except FileNotFoundError:
            self.program.project.reset_calib()
            self.program.gui.info.change_calibration_label()

        # Data
        data = ""
        were_written_data = False
        if os.path.isdir(path + "/measurements"):
            for file in os.listdir(path + "/measurements"):
                if file.endswith(".s2p"):
                    path = (os.path.join(path + "/measurements", file))
                    try:
                        with open(path) as f:
                            for line in f:
                                data += line
                        if not were_written_data:
                            params = self.program.project.get_params_string_from_data(data)
                            s11 = False
                            s12 = False
                            s22 = False
                            s21 = False
                            params_value = params.split(" ")
                            for item in params_value:
                                if item == "S11":
                                    s11 = True
                                if item == "S12":
                                    s12 = True
                                if item == "S22":
                                    s22 = True
                                if item == "S21":
                                    s21 = True
                            self.program.project.reset_data(s11, s12, s21, s22)
                            self.program.settings.set_parameters(params)
                        if self.program.project.exists_data():
                            data = data.strip()
                            self.program.project.data.add_measurement(data)
                            were_written_data = True
                    except FileNotFoundError:
                        pass
                data = ""
            if were_written_data:
                self.program.gui.info.change_data_label()
                return
        self.program.project.reset_data()
        self.program.gui.info.change_data_label()
        return

    def save_calib(self, path):
        # name = "calibration"
        # file_path = os.path.join(path, name)
        file_path = path
        if os.path.exists(file_path):
            now = datetime.now()
            time = datetime.timestamp(now)
            date = datetime.fromtimestamp(time)
            date = str(date)
            date = date.replace(":", "-")
            file_path = file_path[:-4]
            file_path += " " + str(date)

        if file_path[-4:] != ".txt":
            file_path += ".txt"

        # Calibration
        calibration = self.program.project.get_calibration()
        if calibration is not None:
            f = open(file_path, "w")
            f.write(calibration)
            f.close()
            return True
        else:
            return False

    def load_calib(self, path):
        # Calibration
        # filepath = path + "/" + "calibration.txt"
        filepath = path
        try:
            calibration = ""
            with open(filepath) as f:
                for line in f:
                    calibration += line
            self.program.project.set_calibration(calibration)
            return True
        except FileNotFoundError:
            self.program.project.reset_calib()
            self.program.gui.info.change_calibration_label()
            return False

    def save_last_measurement(self):
        if not self.program.project.exists_data():
            return
        filepath = self.autosave_path + "\\" + self.autosave_name + "\\measurements"

        meranie = self.program.project.data.get_number_of_measurements()
        if not os.path.exists(filepath):
            os.mkdir(filepath)

        data = self.program.project.data.print_measurement(meranie-1)
        if data is not None:
            name = "measurement" + str(meranie) + ".s2p"
            file_path = os.path.join(filepath, name)
            f = open(file_path, "w")
            f.write(data)
            f.close()
