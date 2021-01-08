class Settings:
    def __init__(self):
        # defaults:
        self.address = 16
        self.port1 = 0.0
        self.port2 = 0.0
        self.vel_factor = 1.0
        self.freq_unit = "GHz"
        self.freq_start = 1.0
        self.freq_stop = 2.0
        self.points = 201
        self.parameter_format = "RI"
        self.parameters = "S21"   # TODO checknut defaults
        self.continuous = False

    def set_address(self, address):
        self.address = address

    def set_port1(self, port1):
        self.port1 = port1

    def set_port2(self, port2):
        self.port2 = port2

    def set_vel_factor(self, vel_factor):
        self.vel_factor = vel_factor

    def set_freq_unit(self, freq_unit):
        self.freq_unit = freq_unit

    def set_freq_start(self, freq_start):
        self.freq_start = freq_start

    def set_freq_stop(self, freq_stop):
        self.freq_stop = freq_stop

    def set_points(self, points):
        self.points = points

    def set_parameter_format(self, parameter_format):
        self.parameter_format = parameter_format

    def set_parameters(self, parameters):
        self.parameters = parameters

    def set_continuous(self, continuous):
        self.continuous = continuous

    def get_address(self):
        return self.address

    def get_port1(self):
        return self.port1

    def get_port2(self):
        return self.port2

    def get_vel_factor(self):
        return self.vel_factor

    def get_freq_unit(self):
        return self.freq_unit

    def get_freq_start(self):
        return self.freq_start

    def get_freq_stop(self):
        return self.freq_stop

    def get_points(self):
        return self.points

    def get_parameter_format(self):
        return self.parameter_format

    def get_parameters(self):
        return self.parameters

    def get_continuous(self):
        return self.continuous
