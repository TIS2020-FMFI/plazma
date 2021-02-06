class Settings:
    default_address = 16
    default_port1 = 0
    default_port2 = 0
    default_vel_factor = 1.00
    default_freq_unit = "GHz"
    default_freq_start = 1.0
    default_freq_stop = 2.0
    default_points = 201
    default_parameter_format = "MA"
    default_parameters = ""
    default_continuous = False

    def __init__(self):
        # defaults:
        self.address = self.default_address
        self.port1 = self.default_port1
        self.port2 = self.default_port2
        self.vel_factor = self.default_vel_factor
        self.freq_unit = self.default_freq_unit
        self.freq_start = self.default_freq_start
        self.freq_stop = self.default_freq_stop
        self.points = self.default_points
        self.parameter_format = self.default_parameter_format
        self.parameters = self.default_parameters
        self.continuous = self.default_continuous

    def print_settings(self):
        result = ""
        result += "address = " + str(self.get_address()) + "\n"
        result += "port1 = " + str(self.get_port1()) + "\n"
        result += "port2 = " + str(self.get_port2()) + "\n"
        result += "vel_factor = " + str(self.get_vel_factor()) + "\n"
        result += "freq_unit = " + str(self.get_freq_unit()) + "\n"
        result += "freq_start = " + str(self.get_freq_start()) + "\n"
        result += "freq_stop = " + str(self.get_freq_stop()) + "\n"
        result += "points = " + str(self.get_points()) + "\n"
        result += "parameters = " + str(self.get_parameters()) + "\n"
        result += "parameter_format = " + str(self.get_parameter_format()) + "\n"
        result += "continuous = " + str(self.get_continuous())
        return result

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
