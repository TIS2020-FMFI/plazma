from tkinter import Frame
import matplotlib
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk
import matplotlib.pyplot as plt
from smithplot_fixed import SmithAxes
import threading
import queue
import time

matplotlib.use('TkAgg')


class Graphs:
    def __init__(self, gui, program, a, b):
        self.gui = gui
        self.program = program
        self.type = "XYY"
        self.s_param = "S11"
        self.a = a
        self.b = b
        self.empty = True
        self.last_type = self.type

        self.fig, self.ax1 = plt.subplots(1, figsize=(a, b))
        self.ax2 = self.ax1.twinx()

        self.ax2.format_coord = self.make_format(self.ax2, self.ax1)

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

        self.function_queue = queue.Queue()
        self.work_thread = threading.Thread(target=self.execute_functions)
        self.work_thread.daemon = True
        self.work_thread.start()

    def execute_functions(self):
        while True:
            if self.function_queue.empty():
                time.sleep(0.2)
            else:
                print("not sleeeeeeeeeeeeeep")
                function = self.function_queue.get()
                print(function)
                while not self.function_queue.empty():
                    function = self.function_queue.get()
                exec(function)

    def set_type(self, type1):
        self.type = type1

    def set_s_param(self, s_param):
        self.s_param = s_param

    def draw_measurement(self, measurement_index=-1):
        self.function_queue.put(f"self.thread_draw_measurement({measurement_index})")

    def thread_draw_measurement(self, measurement_index=-1):
        if measurement_index == -1 and self.empty and self.type == self.last_type:
            return
        temp = None
        if measurement_index != -1:
            temp = self.program.get_data_for_graph(measurement_index, self.s_param)

        if temp is None and self.empty and self.type == self.last_type:
            return
        if temp is None:
            self.empty = True
        else:
            self.empty = False

        if self.type == "XYY":
            self.last_type = self.type
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
            self.program.gui.window.after_idle(lambda: self.run_draw_measurement(temp, y1, y2))
        else:
            self.last_type = self.type
            val1 = []
            if measurement_index != -1:
                temp = self.program.get_data_for_graph(measurement_index, self.s_param)
                if temp is not None:
                    freq, y1, y2 = temp
                    for i in range(len(y1)):
                        val1.append(y1[i] + y2[i] * 1j)

            self.program.gui.window.after_idle(lambda: self.run_draw_measurement(val1))

    def run_draw_measurement(self, data, y1=None, y2=None):
        if self.type == "XYY":
            plt.close(self.fig)
            print("PREKRESLUJEM  1.5")
            self.canvas.get_tk_widget().grid_forget()
            self.toolbar.grid_forget()
            self.fig, self.ax1 = plt.subplots(1, figsize=(self.a, self.b))
            self.ax2 = self.ax1.twinx()

            self.ax2.format_coord = self.make_format(self.ax2, self.ax1)

            print("PREKRESLUJEM  2")
            color1 = 'tab:red'
            color2 = 'tab:blue'
            self.ax1.set_xlabel('Frequency')
            self.ax1.set_ylabel(y1, color=color1)
            self.ax2.set_ylabel(y2, color=color2)
            print("PREKRESLUJEM  3")

            if data is not None:
                freq, y1, y2 = data
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
            self.fig, self.ax1 = plt.subplots(1, figsize=(self.a, self.b))
            self.fig.clear()

            self.ax2 = self.fig.add_subplot(111, projection='smith')
            if not data:
                data.append(0)
            self.ax2.plot(data, color="red", datatype=SmithAxes.S_PARAMETER)

            self.canvas = FigureCanvasTkAgg(self.fig, master=self.gui)

            self.plot_widget = self.canvas.get_tk_widget()
            self.plot_widget.grid(row=0, column=0)

            self.toolbarFrame.destroy()
            self.toolbarFrame = Frame(master=self.gui)
            self.toolbarFrame.grid(row=1, column=0)
            self.toolbar = NavigationToolbar2Tk(self.canvas, self.toolbarFrame)

    def draw_measurement(self, measurement_index=-1):
        if measurement_index == -1 and self.empty and self.type == self.last_type:
            return
        temp = None
        if measurement_index != -1:
            temp = self.program.get_data_for_graph(measurement_index, self.s_param)

        if temp is None and self.empty and self.type == self.last_type:
            return
        if temp is None:
            self.empty = True
        else:
            self.empty = False


        if self.type == "XYY":
            self.last_type = self.type

            plt.close(self.fig)
            self.canvas.get_tk_widget().grid_forget()
            self.toolbar.grid_forget()
            self.fig, self.ax1 = plt.subplots(1, figsize=(self.a, self.b))
            self.ax2 = self.ax1.twinx()

            self.ax2.format_coord = self.make_format(self.ax2, self.ax1)

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
            D2R = 0.0174532925
            self.param_format = self.program.settings.get_parameter_format()
            self.last_type = self.type
            plt.close(self.fig)
            self.fig, self.ax1 = plt.subplots(1, figsize=(self.a, self.b))
            self.fig.clear()
            val1 = []
            if measurement_index != -1:
                temp = self.program.get_data_for_graph(measurement_index, self.s_param)
                if temp is not None:
                    freq, y1, y2 = temp
                    for i in range(len(y1)):
                        if self.param_format == "MA":
                            print("kreslim MA to real")
                            ang = y2[i] * D2R
                            mag = y1[i]
                            real = math.cos(ang) * mag;
                            imag = math.sin(ang) * mag;
                        elif self.param_format == "DB":
                            print("kreslim db to real")
                            ang = y2[i] * D2R;
                            mag = pow(10.0, y1[i] / 20.0);
                            real = math.cos(ang) * mag;
                            imag = math.sin(ang) * mag;
                        else:
                            real = y1[i]
                            imag = y2[i]

                        val1.append(real + imag * 1j)

            SmithAxes.update_scParams({"axes.normalize.label": False})
            SmithAxes.update_scParams({"plot.marker.start": None})
            SmithAxes.update_scParams({"plot.marker.default": None})
            self.ax2 = self.fig.add_subplot(111, projection='smith')
            if not val1:
                val1.append(0)
            self.ax2.plot(val1, color="red", datatype=SmithAxes.S_PARAMETER)
            self.canvas = FigureCanvasTkAgg(self.fig, master=self.gui)
            self.fig.tight_layout()
            self.plot_widget = self.canvas.get_tk_widget()
            self.plot_widget.grid(row=0, column=0)

            self.toolbarFrame.destroy()
            self.toolbarFrame = Frame(master=self.gui)
            self.toolbarFrame.grid(row=1, column=0)
            self.toolbar = NavigationToolbar2Tk(self.canvas, self.toolbarFrame)

    def make_format(self, current, other):
        def format_coord(x, y):
            display_coord = current.transData.transform((x, y))
            inv = other.transData.inverted()
            ax_coord = inv.transform(display_coord)
            coords = [ax_coord, (x, y)]
            return ('L: {:<}\nR: {:<}'
                    .format(*['({:.5f}, {:.7f})'.format(x, y) for x, y in coords]))
        return format_coord
