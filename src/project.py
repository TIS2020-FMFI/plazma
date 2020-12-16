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
        if self.state is None:
            return ""
        # None?

        return self.state

    def set_calibration(self, calibration):
        self.type = self.program.adapter.get_calibration_type()
        self.calibration = calibration

    def get_calibration(self):
        if self.calibration is None:
            return ""
        # None?

        return self.calibration

    # def adjust_calibration(self, port1, port2, velocity):
    #     # kontrola ci nieco z toho uz nie je nastavene
    #     # poslat do pristroja
    #     # nastavit self.port1 = port1...
    #     pass

    class Data:
        def __init__(self):
            self.number_of_measurements = 0
            self.number_of_parameters = 0
            self.parameters = None   #  dict()  self.parameters["S11"] = True
            self.measurements_list = []  # [(hlavicka1, meranie1_list), (hlavicka2, meranie2_list), ...]
            self.data_type = None  # real/imag....

        def add_measurement(self, data):
            # odvojit data od hlavicky( to co zacina s '!')
            hlavicka = []  # [riadok1_string, riadok2_string...]

            data_list = []  # list tuplov, teda kazdy prvok bude riadok dat
                            # [1 => (freq1, s11_1, s11_2, s12_1, s12_2), 2 => (freq2...)]
            # measurements_list.append((hlavicka, data_list))
            # number++
            pass

        def set_data_type(self, type):
            pass

        def set_parameters(self, number):
            pass




