import tkinter as tk
from calibGUI import CalibrationGui
from gpibGUI import GpibGui
from instStateGUI import InstStateGui
from graphGUI import GraphsGui
from projectGUI import ProjectGui, InfoGui
from sweepGUI import SweepGui


class Gui:
    def __init__(self, program):
        self.window = tk.Tk()
        self.window.title("HP - 8753B")
        self.window.config(bg="white")
        self.connect = False
        self.program = program

        self.create_widgets()

    def create_widgets(self):

        self.window['padx'] = 5
        self.window['pady'] = 5

        # GPIB
        self.gpib = GpibGui(self)

        # INSTRUMENT STATE
        self.state = InstStateGui(self)

        # PROJECT
        self.project = ProjectGui(self)

        # INFO
        self.info = InfoGui(self)

        # CALIBRATIONPART
        self.calibration = CalibrationGui(self)

        # SWEEP
        self.sweep = SweepGui(self)

        # GRAPHS
        self.graphs = GraphsGui(self)



    def state_connected(self):
        self.gpib.gpib_state_connected()
        self.state.instrumentstate_state_connected()
        self.calibration.calibration_state_connected()
        self.sweep.sweep_state_connected()

    def state_disconnected(self):
        self.gpib.gpib_state_disconnected()
        self.state.instrumentstate_state_disconnected()
        self.calibration.calibration_state_disconnected()
        self.sweep.sweep_state_disconnected()


