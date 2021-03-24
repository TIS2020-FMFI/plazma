import tkinter as tk
from calibGUI import CalibrationGui
from gpibGUI import GpibGui
from instStateGUI import InstStateGui
from graphGUI import GraphsGui
from projectGUI import ProjectGui, InfoGui
from sweepGUI import SweepGui
from win32api import GetSystemMetrics


class Gui:
    def __init__(self, program):
        self.window = tk.Tk()
        self.window.title("HP - 8753B")
        self.window.config(bg="white")
        self.connect = False
        self.program = program

        self.window['padx'] = 5
        self.window['pady'] = 5

        self.resize_by_screen()

        # Widgets
        self.gpib = GpibGui(self)
        self.state = InstStateGui(self)
        self.project = ProjectGui(self)
        self.info = InfoGui(self)
        self.calibration = CalibrationGui(self)
        self.sweep = SweepGui(self)
        self.graphs = GraphsGui(self)

    def resize_by_screen(self):
        if GetSystemMetrics(0) in range(1640, 1925):
            self.title_font_size = 16
            self.label_font = 13
            self.label_font_small = 11
            self.line_count = 53
            self.info_weight = 295
            self.info_height = 207
            self.pady_1 = 10
            self.pady_2 = 35
            self.padx = 10
            self.fig_a = 6
            self.fig_b = 3.34
            self.fig_padx = 520
            self.sweep_pady = 20
            self.minus = 0
            self.project_weight = 29
        elif GetSystemMetrics(0) in range(1480, 1641):
            self.title_font_size = 13
            self.label_font = 10
            self.label_font_small = 8
            self.line_count = 45
            self.info_weight = 255
            self.info_height = 210
            self.pady_1 = 5
            self.pady_2 = 20
            self.padx = 5
            self.fig_a = 4.4
            self.fig_b = 2.9
            self.fig_padx = 400
            self.sweep_pady = 20
            self.minus = 5
            self.project_weight = 32
        elif GetSystemMetrics(0) in range(1380, 1480):
            self.title_font_size = 13
            self.label_font = 10
            self.label_font_small = 8
            self.line_count = 45
            self.info_weight = 255
            self.info_height = 202
            self.pady_1 = 5
            self.pady_2 = 30
            self.padx = 5
            self.fig_a = 3.3
            self.fig_b = 2.85
            self.fig_padx = 300
            self.sweep_pady = 20
            self.minus = 0
            self.project_weight = 32
        else:
            self.title_font_size = 8
            self.label_font = 8
            self.label_font_small = 8
            self.line_count = 45
            self.info_weight = 255
            self.info_height = 190
            self.pady_1 = 0
            self.pady_2 = 20
            self.padx = 5
            self.fig_a = 3.0
            self.fig_b = 2.5
            self.fig_padx = 300
            self.sweep_pady = 20
            self.minus = 15
            self.project_weight = 40

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
