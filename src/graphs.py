from matplotlib import pylab
# import skrf as rf
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import LogNorm


class Graphs:

    def __init__(self, gui, program):
        self.gui = gui
        self.program = program

        self.type = "XYY"
        self.s_param = "S11"
        self.autoscale = False
        self.y1_min = None
        self.y1_max = None
        self.y2_min = None
        self.y2_max = None

        # figure = pylab.figure(1)  #,figsize=(2.9, 2.2), dpi=140)
        # canvas = FigureCanvasTkAgg(figure, master=gui)

        # Create some mock data
        # t = np.arange(0.01, 10.0, 0.01)
        # data1 = np.exp(t)
        # data2 = np.sin(2 * np.pi * t)

        self.fig, self.ax1 = plt.subplots(1, figsize=(4, 3))
        color1 = 'tab:red'
        self.ax1.set_xlabel('Frequency')
        self.ax1.set_ylabel('y1', color=color1)
        self.ax1.tick_params(axis='y', labelcolor=color1)

        self.ax2 = self.ax1.twinx()  # instantiate a second axes that shares the same x-axis

        color2 = 'tab:blue'
        self.ax2.set_ylabel('y2', color=color2)  # we already handled the x-label with ax1
        self.ax2.tick_params(axis='y', labelcolor=color2)

        self.fig.tight_layout()  # otherwise the right y-label is slightly clipped
        # plt.show()

        self.canvas = FigureCanvasTkAgg(self.fig, master=gui)



        # plt.plot()
        self.plot_widget = self.canvas.get_tk_widget()
        self.plot_widget.grid(row=0, column=0)

    def set_type(self, type):
        self.type = type

    def set_s_param(self, s_param):
        self.s_param = s_param

    def set_autoscale(self, autoscale):
        self.autoscale = autoscale

    def set_y1_min(self, y1_min):
        self.y1_min = y1_min

    def set_y1_max(self, y1_max):
        self.y1_max = y1_max

    def set_y2_min(self, y2_min):
        self.y2_min = y2_min

    def set_y2_max(self, y2_max):
        self.y2_max = y2_max

    def draw_measurement(self, measurement_index=-1):
        self.ax1.clear()
        self.ax2.clear()

        color1 = 'tab:red'
        color2 = 'tab:blue'
        self.ax1.set_xlabel('Frequency')
        self.ax1.set_ylabel('y1', color=color1)
        self.ax2.set_ylabel('y2', color=color2)

        if measurement_index != -1:
            temp = self.program.get_data_for_graph(measurement_index, self.s_param)
            if temp is not None:
                freq, y1, y2 = temp

                self.ax1.plot(freq, y1, color=color1)
                self.ax2.plot(freq, y2, color=color2)

        self.canvas = FigureCanvasTkAgg(self.fig, master=self.gui)

        self.plot_widget = self.canvas.get_tk_widget()
        self.plot_widget.grid(row=0, column=0)

