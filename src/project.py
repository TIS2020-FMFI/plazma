class State:
    def __init__(self, stav):
        self.stav = stav



class Calibration:
    def __init__(self, adapter):
        self.calibration = None
        self.typ = None
        self.port1 = 0
        self.port2 = 0
        self.velocity_factor = 0
        self.adapter = adapter

    def get_calibration(self):
        self.adapter.get_calibration()

    def set_calibration(self):
       self.adapter.set_calibration(self)

    def adjust_calibration(self, port1, port2, velocity):
        ...



class FileManager:
    # settings, save, load
    pass


class Data:
    number_of_measurements = 1
    data = []   # [meranie1, meranie2, meranie3...]
