class Project:
    def __init__(self, program):
        self.START_OF_THE_LINE_WITH_PARAMS = "!    Params:"
        self.program = program
        self.state = None
        self.calibration = None
        self.calib_type = None
        self.data = None    # typu Data()

    def set_state(self, state):
        self.state = state

    def get_state(self):
        return self.state

    def reset_state(self):
        self.state = None

    def reset_calib(self):
        self.calibration = None

    def get_calibration_type(self):
        return self.calib_type

    def set_calibration(self, calibration):
        calibration = calibration.strip()
        index = calibration.find("\n")
        self.calib_type = calibration[:index]
        self.calibration = calibration

    def get_calibration(self):
        return self.calibration

    def exists_data(self):
        if self.data is None:
            return False
        return True
 
    def reset_data(self, param_11=False, param_12=False, param_21=False, param_22=False):
        if param_11 or param_12 or param_21 or param_22:
            self.data = self.Data(param_11, param_12, param_21, param_22)
        else:
            self.data = None
        self.program.gui.graphs.reset_all_graphs()

    def get_params_string_from_data(self, data):
        data_lines = data.split("\n")
        for line in data_lines:
            if "!    Params:" in line:
                line = line[len(self.START_OF_THE_LINE_WITH_PARAMS):]
                line = line.strip()
                return line
        return ""

    class Data:
        def __init__(self, param_11, param_12, param_21, param_22):
            self.number_of_measurements = 0
            self.parameters = {"S11": param_11, "S12": param_12, "S21": param_21,
                               "S22": param_22}
            self.measurements_list = []  # [(hlavicka1, meranie1_dict), (hlavicka2, meranie2_dict), ...]

        def get_number_of_measurements(self):
            return self.number_of_measurements

        def add_measurement(self, data):
            lines = data.split('\n')
            header = []  # [riadok1_string, riadok2_string...]
            data_dict = {}  # slovník1[frekvencia] = slovník2, slovník2["S11"] = (hodnota1, hodnota2)
            
            true_params = []
            for key, val in self.parameters.items():
                if val:
                    true_params.append(key)
            true_params.sort()
            if "S12" in true_params and "S21" in true_params:
                i_s12 = true_params.index("S12")
                i_s21 = true_params.index("S21")
                true_params[i_s12] = "S21"
                true_params[i_s21] = "S12"
                    
            for line in lines:
                line.strip()
                if len(line) == 0:
                    continue
                if line[0] in ('!', '#'):
                    header.append(line)
                else:
                    line_list = line.split()
                    values = {}
                    index = 1
                    for param in true_params:
                        values[param] = (line_list[index], line_list[index + 1])
                        index += 2
                    data_dict[line_list[0]] = values

            self.measurements_list.append(('\n'.join(header), data_dict))
            self.number_of_measurements += 1

        def get_measurement(self, s_param, measurement_index=0):
            # vráti slovník pre grafy, measurement[frekvencia] = (hodnota1, hodnota2)
            measurement = {}
            try:
                for key, val in self.measurements_list[measurement_index][1].items():
                    measurement[float(key)] = (float(val[s_param][0]), float(val[s_param][1]))
                return measurement
            except (KeyError, IndexError):
                return None

        def print_measurement(self, measurement_index=0):
            result = self.measurements_list[measurement_index][0] + '\n'
            for key, val in self.measurements_list[measurement_index][1].items():
                result += key
                for value1, value2 in val.values():
                    result += ' ' + value1 + ' ' + value2
                result += '\n'
            return result
