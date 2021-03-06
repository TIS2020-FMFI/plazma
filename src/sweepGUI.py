import tkinter as tk
import tkinter.font as tk_font
import data_validation


class SweepGui:
    def __init__(self, main_gui):
        self.gui = main_gui
        self.current_frame = 0
        self.on_last_frame = True

        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.gui.title_font_size, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=self.gui.label_font)
        widget_port_font = tk_font.Font(family="Tw Cen MT", size=self.gui.label_font_small)
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=self.gui.label_font)

        sweep_frame = tk.LabelFrame(self.gui.window, text="SWEEP", fg="#323338", bg='#f2f3fc',
                                    font=widget_title_font, relief=tk.RIDGE)
        sweep_frame.grid(row=1, column=2, rowspan=3, sticky=tk.W, pady=(self.gui.pady_2, 0), padx=(self.gui.padx, 0))

        freq_measure_label = tk.Label(sweep_frame, text="Frequency Measure", fg='#323338', bg="#f2f3fc")
        freq_measure_label["font"] = widget_label_font
        freq_measure_label.grid(row=0, column=0, columnspan=4, padx=(50, 50))

        self.freq_variable = tk.IntVar()
        self.mhz_radiobutton = tk.Radiobutton(sweep_frame, text="MHz", bg='#f2f3fc', fg="#323338",
                                              variable=self.freq_variable, value=0, font=widget_label_font)
        self.ghz_radiobutton = tk.Radiobutton(sweep_frame, text="GHz", bg='#f2f3fc', fg="#323338",
                                              variable=self.freq_variable, value=1, font=widget_label_font)

        self.mhz_radiobutton.grid(row=1, column=0, columnspan=2, sticky=tk.W, pady=(10, 0), padx=20)
        self.ghz_radiobutton.grid(row=1, column=2, columnspan=2, sticky=tk.E, pady=(10, 0), padx=(0, 40))

        self.mhz_radiobutton.select()
        self.ghz_radiobutton.deselect()
        if self.gui.program.settings.get_freq_unit() == "MHz":
            self.freq_variable.set(0)
        else:
            self.freq_variable.set(1)

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=2, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        start_label = tk.Label(sweep_frame, text="Start:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        start_label.grid(row=3, column=0, sticky=tk.W, padx=(20, 0), pady=(15, 0))

        self.start_entry = tk.Entry(sweep_frame, width=5)
        self.start_entry.grid(row=3, column=1, sticky=tk.W, pady=(15, 0))
        self.start_entry.insert(tk.END, self.gui.program.settings.get_freq_start())
        self.start_entry["font"] = widget_label_font

        stop_label = tk.Label(sweep_frame, text="Stop:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        stop_label.grid(row=3, column=2, sticky=tk.W, pady=(15, 0), padx=(60, 5))

        self.stop_entry = tk.Entry(sweep_frame, width=5)
        self.stop_entry.grid(row=3, column=3, sticky=tk.W, padx=(0, 30), pady=(15, 0))
        self.stop_entry.insert(tk.END, self.gui.program.settings.get_freq_stop())
        self.stop_entry["font"] = widget_label_font

        points_label = tk.Label(sweep_frame, text="Points:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        points_label.grid(row=4, column=0, sticky=tk.W, padx=(20, 0), pady=(5, 0))

        self.points_entry = tk.Entry(sweep_frame, width=5)
        self.points_entry.grid(row=4, column=1, sticky=tk.W, pady=(5, 0))
        self.points_entry.insert(tk.END, self.gui.program.settings.get_points())
        self.points_entry["font"] = widget_label_font

        points_label = tk.Label(sweep_frame, text="(/1601)", fg="#323338", bg='#f2f3fc', font=widget_port_font)
        points_label.grid(row=4, column=2, sticky=tk.W, pady=(5, 0))

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=5, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        s_param_label = tk.Label(sweep_frame, text="S - parameters", fg='#323338', bg="#f2f3fc")
        s_param_label["font"] = widget_label_font
        s_param_label.grid(row=6, column=0, columnspan=4, padx=(50, 50))

        self.s11 = tk.IntVar()
        self.s11_checkbutton = tk.Checkbutton(sweep_frame, variable=self.s11, text="S11", font=widget_port_font,
                                              bg='#f2f3fc')
        self.s11_checkbutton.grid(row=7, column=1, columnspan=2, sticky=tk.W, pady=(5, 0), padx=(30, 0))

        self.s12 = tk.IntVar()
        self.s12_checkbutton = tk.Checkbutton(sweep_frame, variable=self.s12, text="S12", font=widget_port_font,
                                              bg='#f2f3fc')
        self.s12_checkbutton.grid(row=7, column=2, columnspan=2, sticky=tk.W, pady=(5, 0), padx=(20, 0))

        self.s21 = tk.IntVar()
        self.s21_checkbutton = tk.Checkbutton(sweep_frame, variable=self.s21, text="S21", font=widget_port_font,
                                              bg='#f2f3fc')
        self.s21_checkbutton.grid(row=8, column=1, columnspan=2, sticky=tk.W, pady=5, padx=(30, 0))

        self.s22 = tk.IntVar()
        self.s22_checkbutton = tk.Checkbutton(sweep_frame, variable=self.s22, text="S22", font=widget_port_font,
                                              bg='#f2f3fc')
        self.s22_checkbutton.grid(row=8, column=2, columnspan=2, sticky=tk.W, pady=5, padx=(20, 0))

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=9, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.measure_variable = tk.IntVar()
        self.ma_radiobutton = tk.Radiobutton(sweep_frame, text="magnitude-angle", bg='#f2f3fc', fg="#323338",
                                             variable=self.measure_variable, value=0, font=widget_label_font)
        self.db_radiobutton = tk.Radiobutton(sweep_frame, text="db-angle", bg='#f2f3fc', fg="#323338",
                                             variable=self.measure_variable, value=1, font=widget_label_font)
        self.ri_radiobutton = tk.Radiobutton(sweep_frame, text="real-imaginary", bg='#f2f3fc', fg="#323338",
                                             variable=self.measure_variable, value=2, font=widget_label_font)

        self.ma_radiobutton.grid(row=10, column=0, columnspan=4, sticky=tk.W, pady=(10, 0), padx=20)
        self.db_radiobutton.grid(row=11, column=0, columnspan=4, sticky=tk.W, pady=(0, 0), padx=20)
        self.ri_radiobutton.grid(row=12, column=0, columnspan=4, sticky=tk.W, pady=(0, 0), padx=20)

        param = self.gui.program.settings.get_parameter_format()
        if param == "MA":
            self.measure_variable.set(0)
        elif param == "DB":
            self.measure_variable.set(1)
        else:
            self.measure_variable.set(2)

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5",
                              bg="#f2f3fc")
        line_label.grid(row=13, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.continuous = tk.IntVar()
        self.continuous_checkbutton = tk.Checkbutton(sweep_frame, variable=self.continuous, text="continuous",
                                                     font=widget_port_font, bg='#f2f3fc')
        self.continuous_checkbutton.grid(row=14, column=0, columnspan=2, sticky=tk.W, pady=(5, 0), padx=(30, 0))
        self.continuous_checkbutton["state"] = tk.DISABLED
        if self.gui.program.settings.get_continuous():
            self.continuous.set(1)
        else:
            self.continuous.set(0)

        self.autosave = tk.IntVar()
        self.autosave_checkbutton = tk.Checkbutton(sweep_frame, variable=self.autosave, text="autosave",
                                                   font=widget_port_font, bg='#f2f3fc')
        self.autosave_checkbutton.grid(row=14, column=2, columnspan=2, sticky=tk.W, pady=(5, 0))
        self.autosave_checkbutton["state"] = tk.DISABLED

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=15, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.frame_label = tk.Label(sweep_frame, text="Frame:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.frame_label.grid(row=16, column=0, sticky=tk.W, pady=(2, 0), padx=(30, 0))

        self.frame_entry = tk.Entry(sweep_frame, width=3, fg="black", font=widget_label_font)
        self.frame_entry.grid(row=16, column=1, sticky=tk.W, pady=(15, 10))
        self.frame_entry.insert(tk.END, "1")

        self.max_label = tk.Label(sweep_frame, text="/10", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.max_label.grid(row=16, column=1, sticky=tk.W, padx=(30, 0))

        self.frame_first_button = tk.Button(sweep_frame, text="<<", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                            command=self.first_frame)
        self.frame_first_button.grid(row=16, column=2, columnspan=2, pady=2, sticky=tk.W)

        self.frame_down_button = tk.Button(sweep_frame, text="<", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                           command=self.previous_frame)
        self.frame_down_button.grid(row=16, column=2, pady=5, padx=(40, 0), sticky=tk.W)

        self.frame_up_button = tk.Button(sweep_frame, text=">", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                         command=self.next_frame)
        self.frame_up_button.grid(row=16, column=2, padx=(70, 0), pady=2, sticky=tk.W)

        self.frame_last_button = tk.Button(sweep_frame, text=">>", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                           command=self.last_frame)
        self.frame_last_button.grid(row=16, column=2, columnspan=2, pady=2, padx=(100, 0), sticky=tk.W)

        self.run_button = tk.Button(sweep_frame, text="Run", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                    width=15, command=self.start_measure)
        self.run_button.grid(row=17, column=1, columnspan=3, pady=20-self.gui.minus, sticky=tk.W)
        self.run_button["state"] = tk.DISABLED

        self.reset_frame()
        sweep_frame["width"] = 4000
        sweep_frame["height"] = 4000

    def start_measure(self):
        measure = True

        start = self.start_entry.get()
        stop = self.stop_entry.get()

        validation1 = data_validation.validate_float(self.start_entry.get())
        validation2 = data_validation.validate_float(self.stop_entry.get())
        if validation1 and validation2:
            if data_validation.validate_start_stop(start, stop, self.freq_variable.get()):
                self.start_entry["bg"] = "white"
                self.stop_entry["bg"] = "white"
            else:
                self.start_entry["bg"] = "#cf5f5f"
                self.stop_entry["bg"] = "#cf5f5f"
                measure = False
        elif validation1:
            self.start_entry["bg"] = "white"
            self.stop_entry["bg"] = "#cf5f5f"
            measure = False
        elif validation2:
            self.start_entry["bg"] = "#cf5f5f"
            self.stop_entry["bg"] = "white"
            measure = False
        else:
            self.start_entry["bg"] = "#cf5f5f"
            self.stop_entry["bg"] = "#cf5f5f"
            measure = False

        if data_validation.validate_points(self.points_entry.get()):
            self.points_entry["bg"] = "white"
        else:
            self.points_entry["bg"] = "#cf5f5f"
            measure = False

        if measure:
            if self.run_button["text"] == "Run":
                self.run_measure()
            else:
                self.stop_measure()

    def stop_measure(self):
        self.gui.program.end_measurement()

    def run_measure(self):        
        self.run_button["state"] = tk.DISABLED
        self.gui.program.project.reset_data()
        autosave = False
        if self.autosave.get() == 1:
            autosave = True
            if not self.gui.project.save():
                self.run_button["state"] = tk.NORMAL 
                return
        self.reset_frame()        
        self.gui.info.waiting_data_label()
        self.send_settings()
        if self.continuous.get() == 0:
            self.disable_measurement_checkboxes()
            self.gui.program.queue_function(f"measure({autosave})")            
        else:
            self.run_button["text"] = "Stop"
            self.disable_measurement_checkboxes()
            self.gui.program.queue_function(f"start_measurement({autosave})")
            self.run_button["state"] = tk.NORMAL            

    def change_run(self):
        self.run_button["text"] = "Run"

    def refresh_points(self):
        self.points_entry.delete(0, tk.END)
        self.points_entry.insert(tk.END, self.gui.program.settings.get_points())

    def send_settings(self):
        unit = "MHz"
        if self.freq_variable.get() == 1:
            unit = "GHz"
        self.gui.program.settings.set_freq_unit(unit)

        self.gui.program.settings.set_freq_start(float(self.start_entry.get()))
        self.gui.program.settings.set_freq_stop(float(self.stop_entry.get()))
        self.gui.program.settings.set_points(int(float(self.points_entry.get())))  # musia byt oba

        params = ""
        if self.s11.get():
            params += "S11 "
        if self.s21.get():
            params += "S21 "
        if self.s12.get():
            params += "S12 "
        if self.s22.get():
            params += "S22 "
        self.gui.program.settings.set_parameters(params)

        if self.measure_variable.get() == 0:
            param_format = "MA"
        elif self.measure_variable.get() == 1:
            param_format = "DB"
        else:
            param_format = "RI"
        self.gui.program.settings.set_parameter_format(param_format)

        if self.continuous.get() == 0:
            self.gui.program.settings.set_continuous(False)
        else:
            self.gui.program.settings.set_continuous(True)

        self.gui.program.settings.set_address(self.gui.gpib.address_entry.get())

    def refresh_frame(self):
        if self.gui.program.project.exists_data():
            max_frames = self.gui.program.project.data.get_number_of_measurements()
            if self.on_last_frame:
                self.current_frame = max_frames

            self.frame_entry["state"] = tk.NORMAL
            self.frame_entry.delete(0, tk.END)
            self.frame_entry.insert(tk.END, str(self.current_frame))
            self.frame_entry["state"] = tk.DISABLED
            self.max_label["text"] = " / " + str(max_frames)
            self.frame_down_button["state"] = tk.NORMAL
            self.frame_up_button["state"] = tk.NORMAL
            self.frame_last_button["state"] = tk.NORMAL
            self.frame_first_button["state"] = tk.NORMAL
            self.gui.info.change_data_label()

        else:
            self.reset_frame()
        self.gui.graphs.refresh_all_graphs()

    def reset_frame(self):
        self.current_frame = 0
        self.frame_entry["state"] = tk.NORMAL
        self.frame_entry.delete(0, tk.END)
        self.frame_entry.insert(tk.END, "0")
        self.frame_entry["state"] = tk.DISABLED
        self.max_label["text"] = "/ ---"
        self.frame_down_button["state"] = tk.DISABLED
        self.frame_up_button["state"] = tk.DISABLED
        self.frame_last_button["state"] = tk.DISABLED
        self.frame_first_button["state"] = tk.DISABLED
        self.gui.info.change_data_label()

    def sweep_state_connected(self):
        self.autosave_checkbutton["state"] = tk.NORMAL
        self.continuous_checkbutton["state"] = tk.NORMAL
        self.run_button["state"] = tk.NORMAL

    def sweep_state_disconnected(self):
        self.autosave_checkbutton["state"] = tk.DISABLED
        self.continuous_checkbutton["state"] = tk.DISABLED
        self.run_button["state"] = tk.DISABLED

    def load_project_sweep(self):
        if self.gui.program.settings.get_freq_unit() == "MHz":
            self.freq_variable.set(0)
        else:
            self.freq_variable.set(1)

        self.start_entry.delete(0, tk.END)
        self.start_entry.insert(tk.END, self.gui.program.settings.get_freq_start())
        self.stop_entry.delete(0, tk.END)
        self.stop_entry.insert(tk.END, self.gui.program.settings.get_freq_stop())
        self.points_entry.delete(0, tk.END)
        self.points_entry.insert(tk.END, self.gui.program.settings.get_points())

        params = self.gui.program.settings.get_parameters()
        params_value = params.split(" ")
        if "S11" in params_value:
            self.s11.set(1)
        else:
            self.s11.set(0)
        if "S12" in params_value:
            self.s12.set(1)
        else:
            self.s12.set(0)
        if "S22" in params_value:
            self.s22.set(1)
        else:
            self.s22.set(0)
        if "S21" in params_value:
            self.s21.set(1)
        else:
            self.s21.set(0)

        if self.gui.program.settings.get_parameter_format() == "RI":
            self.measure_variable.set(2)
        elif self.gui.program.settings.get_parameter_format() == "MA":
            self.measure_variable.set(0)
        else:
            self.measure_variable.set(1)

        if self.gui.program.settings.get_continuous():
            self.continuous.set(1)
        else:
            self.continuous.set(0)

        self.refresh_frame()

    def next_frame(self):
        if self.current_frame < self.gui.program.project.data.get_number_of_measurements():
            self.current_frame += 1
            self.refresh_frame()
        else:
            self.on_last_frame = True

    def previous_frame(self):
        self.on_last_frame = False
        if self.current_frame > 1:
            self.current_frame -= 1
            self.refresh_frame()

    def last_frame(self):
        self.on_last_frame = True
        if self.current_frame < self.gui.program.project.data.get_number_of_measurements():
            self.current_frame = self.gui.program.project.data.get_number_of_measurements()
            self.refresh_frame()

    def first_frame(self):
        self.on_last_frame = False
        if self.current_frame > 1:
            self.current_frame = 1
            self.refresh_frame()

    def disable_measurement_checkboxes(self):
        self.continuous_checkbutton["state"] = tk.DISABLED
        self.autosave_checkbutton["state"] = tk.DISABLED

    def enable_measurement_checkboxes(self):
        self.continuous_checkbutton["state"] = tk.NORMAL
        self.autosave_checkbutton["state"] = tk.NORMAL
