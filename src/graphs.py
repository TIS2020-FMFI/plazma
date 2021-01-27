from tkinter import Frame
import matplotlib
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk
import matplotlib.pyplot as plt
from smithplot_fixed import SmithAxes

matplotlib.use('TkAgg')


class Graphs:
    def __init__(self, gui, program):
        self.gui = gui
        self.program = program

        self.type = "XYY"
        self.s_param = "S11"

        self.fig, self.ax1 = plt.subplots(1, figsize=(4.8, 3.35))
        self.ax2 = self.ax1.twinx()

        color1 = 'tab:red'
        color2 = 'tab:blue'
        self.ax1.set_xlabel('Frequency')
        param_format = self.program.settings.get_parameter_format()
        if param_format == "DB":
            y1 = "db"
            y2 = "Angle"
        elif param_format == "RI":
            y1 = "Real"
            y2 = "Imaginary"
        else:
            y1 = "Magnitude"
            y2 = "Angle"
        self.ax1.set_ylabel(y1, color=color1)
        self.ax2.set_ylabel(y2, color=color2)

        self.fig.tight_layout()
        self.canvas = FigureCanvasTkAgg(self.fig, master=gui)

        self.plot_widget = self.canvas.get_tk_widget()
        self.plot_widget.grid(row=0, column=0)

        self.toolbarFrame = Frame(master=self.gui)
        self.toolbarFrame.grid(row=1, column=0)
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.toolbarFrame)

    def set_type(self, type1):
        self.type = type1

    def set_s_param(self, s_param):
        self.s_param = s_param

    def draw_measurement(self, measurement_index=-1):
        if self.type == "XYY":
            plt.close(self.fig)
            self.canvas.get_tk_widget().grid_forget()
            self.toolbar.grid_forget()
            self.fig, self.ax1 = plt.subplots(1, figsize=(4.8, 3.35))
            self.ax2 = self.ax1.twinx()

            color1 = 'tab:red'
            color2 = 'tab:blue'
            self.ax1.set_xlabel('Frequency')
            param_format = self.program.settings.get_parameter_format()
            if param_format == "DB":
                y1 = "db"
                y2 = "Angle"
            elif param_format == "RI":
                y1 = "Real"
                y2 = "Imaginary"
            else:
                y1 = "Magnitude"
                y2 = "Angle"
            self.ax1.set_ylabel(y1, color=color1)
            self.ax2.set_ylabel(y2, color=color2)

            if measurement_index != -1:
                temp = self.program.get_data_for_graph(measurement_index, self.s_param)
                if temp is not None:
                    freq, y1, y2 = temp

                    self.ax1.plot(freq, y1, color=color1)
                    self.ax2.plot(freq, y2, color=color2)

            self.fig.tight_layout()
            self.canvas = FigureCanvasTkAgg(self.fig, master=self.gui)

            self.plot_widget = self.canvas.get_tk_widget()
            self.plot_widget.grid(row=0, column=0)

            self.toolbarFrame.destroy()
            self.toolbarFrame = Frame(master=self.gui)
            self.toolbarFrame.grid(row=1, column=0)
            self.toolbar = NavigationToolbar2Tk(self.canvas, self.toolbarFrame)

        else:
            plt.close(self.fig)
            self.fig, self.ax1 = plt.subplots(1, figsize=(4.8, 3.35))
            self.fig.clear()
            # self.val1 = []
            val1 = []
            if measurement_index != -1:
                temp = self.program.get_data_for_graph(measurement_index, self.s_param)
                if temp is not None:
                    freq, y1, y2 = temp
                    for i in range(len(y1)):
                        # self.val1.append(y1[i] + y2[i] * 1j)
                        val1.append(y1[i] + y2[i] * 1j)

            self.ax2 = self.fig.add_subplot(111, projection='smith')
            # self.ax2.plot(self.val1, color="red", datatype=SmithAxes.S_PARAMETER)
            self.ax2.plot(val1, color="red", datatype=SmithAxes.S_PARAMETER)

            self.canvas = FigureCanvasTkAgg(self.fig, master=self.gui)

            self.plot_widget = self.canvas.get_tk_widget()
            self.plot_widget.grid(row=0, column=0)

            self.toolbarFrame.destroy()
            self.toolbarFrame = Frame(master=self.gui)
            self.toolbarFrame.grid(row=1, column=0)
            self.toolbar = NavigationToolbar2Tk(self.canvas, self.toolbarFrame)
