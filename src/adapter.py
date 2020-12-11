class Adapter:
    def __init__(self, address):
        self.address = address  # address tu alebo v metode?
        self.connected = False
        self.out_queue = None
        self.in_queue = None

    def connect(self):
        pass

    def disconnect(self):
        pass

    def get_state(self):
        return ""

    def set_state(self, state):  # state = string
        pass

    def reset(self):
        pass

    def get_calibration(self):
        pass

    def set_calibration(self, calibration):
        pass

    def set_port1_length(self, value):
        pass

    def set_port2_length(self, value):
        pass

    def set_velocity_factor(self, value):
        pass

    def set_frequency_format(self, value):
        pass

    def set_start_frequency(self, value):
        pass

    def set_stop_frequency(self, value):
        pass

    def set_points(self, value):
        pass

    def set_data_format(self, value):
        pass

    def set_parameters(self, value):
        pass

    def measure(self):
        pass

    def start_measurement(self):
        pass

    def end_measurement(self):
        pass


# open_terminal()
# close_terminal()
# send_command(command)
# ping()
# restart_connection()
# time.sleep() pouzivat vsade po kazdom prikaze alebo nejako inak?
