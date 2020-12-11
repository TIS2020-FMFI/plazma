class State:
    def __init__(self):
        self.stav = ""


class Calibration:
    def __init__(self):
        self.calibration = ""
        self.typ = ""
        self.port1 = 0
        self.port2 = 0
        self.velocity_factor = 0


class FileManager:
    # settings, save, load
    pass


class Data:
    number_of_measurements = 1
    data = []   # [meranie1, meranie2, meranie3...]