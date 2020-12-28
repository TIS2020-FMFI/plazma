import copy

class Project:
    def __init__(self, program):
        self.program = program

        self.state = None   # string

        self.calibration = None  # string
        self.type = None  # string ale iba tie typy co mozu byt
        # self.port1 = 0
        # self.port2 = 0
        # self.velocity_factor = 0

        self.data = None    # typu Data()

    def set_state(self, state):
        self.state = state

    def get_state(self):
        return self.state

    def set_calibration(self, calibration):
        self.type = self.program.adapter.get_calibration_type()
        self.calibration = calibration

    def get_calibration(self):
        return self.calibration

    # def adjust_calibration(self, port1, port2, velocity):
    #     # kontrola ci nieco z toho uz nie je nastavene
    #     # poslat do pristroja
    #     # nastavit self.port1 = port1...
    #     pass
 
    def reset_data(self, param_11 = False, param_12 = False, param_21 = False, param_22 = False):
        # zavolá sa, po stlačení tlačidla run, bude sa volať z Program.py, ktorý bude tiež volať metódu z adapter.py 
        self.data = Data(param_11, param_12, param_21, param_22)
        
        # prípadne nastaviť self.data_type alebo aj number_of_parameters
        
    class Data:
        def __init__(self, param_11, param_12, param_21, param_22):
            self.number_of_measurements = 0
            self.parameters = {}  #  dict()  self.parameters["S11"] = True
            self.parameters["S11"] = param_11
            self.parameters["S12"] = param_12
            self.parameters["S21"] = param_21
            self.parameters["S22"] = param_22
            self.measurements_list = []  # [(hlavicka1, meranie1_list), (hlavicka2, meranie2_list), ...]
            # self.data_type = None  # real/imag....

        def add_measurement(self, data):
            lines = data.split('\n')
            header = []  # [riadok1_string, riadok2_string...]
            data_dict = {}  # slovník1[frekvencia] = slovník2, slovník2["S11"] = (hodnota1, hodnota2)
            
            true_params = []
            for key, val in self.parameters.items():
                if val:
                    true_params.append(key)
            true_params.sort()
                    
            for line in lines:
                line.strip()
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

            self.measurement_list.append(('\n'.join(header), data_dict))
            self.number_of_measurements += 1

        def get_measurement(self, s_param, measurement_index = 0):
            measurement = {}
            for key, val in self.measurements_list[measurement_index][1].items():
                measurement[key] = copy.copy(val[s_param])
            return measurement  # vráti slovník pre grafy, measurement[frekvencia] = (hodnota1, hodnota2)

        def print_measurement(self, measurement_index = 0):
            result = self.measurement_list[measurement_index][0] + '\n'
            for key, val in self.measurement_list[measurement_index][1].items():
                result += key
                for value1, value2 in val.values():
                    result += ' ' + value1 + ' ' + value2
                result += '\n'
            return result  

##        def set_data_type(self, type):
##            pass

