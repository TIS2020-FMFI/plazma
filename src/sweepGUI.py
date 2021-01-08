import tkinter as tk
import tkinter.font as tk_font
import data_validation


class SweepGui:
    def __init__(self, main_gui):
        self.gui = main_gui
        self.create_sweep_gui()

    def create_sweep_gui(self):
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=13)
        widget_port_font = tk_font.Font(family="Tw Cen MT", size=11)
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=13)

        sweep_frame = tk.LabelFrame(self.gui.window, width=1000, height=3000, text="SWEEP", fg="#323338", bg='#f2f3fc',
                                    font=widget_title_font, relief=tk.RIDGE)
        sweep_frame.grid(row=0, column=2, rowspan=8, sticky=tk.NW, padx=6, pady=(255, 0))

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
        self.freq_variable.set(0)

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=2, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        start_label = tk.Label(sweep_frame, text="Start:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        start_label.grid(row=3, column=0, sticky=tk.W, padx=(20, 0), pady=(15, 0))

        self.start_entry = tk.Entry(sweep_frame, width=5)
        self.start_entry.grid(row=3, column=1, sticky=tk.W, pady=(15, 0))
        self.start_entry.insert(tk.END, "0")
        self.start_entry["font"] = widget_label_font

        stop_label = tk.Label(sweep_frame, text="Stop:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        stop_label.grid(row=3, column=2, sticky=tk.W, pady=(15, 0), padx=(60, 5))

        self.stop_entry = tk.Entry(sweep_frame, width=5)
        self.stop_entry.grid(row=3, column=3, sticky=tk.W, padx=(0, 30), pady=(15, 0))
        self.stop_entry.insert(tk.END, "100")
        self.stop_entry["font"] = widget_label_font

        points_label = tk.Label(sweep_frame, text="Points:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        points_label.grid(row=4, column=0, sticky=tk.W, padx=(20, 0), pady=(5, 0))

        self.points_entry = tk.Entry(sweep_frame, width=5)
        self.points_entry.grid(row=4, column=1, sticky=tk.W, pady=(5, 0))
        self.points_entry.insert(tk.END, "200")
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
        self.ma_radiobutton.select()
        self.measure_variable.set(0)
        self.db_radiobutton.deselect()
        self.ri_radiobutton.deselect()

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5",
                              bg="#f2f3fc")
        line_label.grid(row=13, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.continuous = tk.IntVar()
        self.continuous_checkbutton = tk.Checkbutton(sweep_frame, variable=self.continuous, text="continuous",
                                                     font=widget_port_font, bg='#f2f3fc')
        self.continuous_checkbutton.grid(row=14, column=0, columnspan=2, sticky=tk.W, pady=(5, 0), padx=(30, 0))
        self.continuous_checkbutton["command"] = self.visibility

        self.autosave = tk.IntVar()
        self.autosave_checkbutton = tk.Checkbutton(sweep_frame, variable=self.autosave, text="autosave",
                                                   font=widget_port_font, bg='#f2f3fc')
        self.autosave_checkbutton.grid(row=14, column=2, columnspan=2, sticky=tk.W, pady=(5, 0))

        line_label = tk.Label(sweep_frame, text="________________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=15, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.frame_label = tk.Label(sweep_frame, text="Frame:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.frame_label.grid(row=16, column=0, sticky=tk.W, pady=(2, 0), padx=(30, 0))

        self.frame_entry = tk.Entry(sweep_frame, width=3, fg="#838691", font=widget_label_font)
        self.frame_entry.grid(row=16, column=1, sticky=tk.W, pady=(15, 10))
        self.frame_entry.insert(tk.END, "1")

        self.max_label = tk.Label(sweep_frame, text="/10", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.max_label.grid(row=16, column=1, sticky=tk.W, padx=(30, 0))

        self.frame_down_button = tk.Button(sweep_frame, text="<", bg='#bfc6db', fg='#323338', font=widget_button_font)
        self.frame_down_button.grid(row=16, column=2, pady=5, sticky=tk.W)

        self.frame_up_button = tk.Button(sweep_frame, text=">", bg='#bfc6db', fg='#323338', font=widget_button_font)
        self.frame_up_button.grid(row=16, column=2, padx=(30, 0), pady=2, sticky=tk.W)

        self.frame_last_button = tk.Button(sweep_frame, text=">>", bg='#bfc6db', fg='#323338', font=widget_button_font)
        self.frame_last_button.grid(row=16, column=2, columnspan=3, pady=2, padx=(60, 0), sticky=tk.W)

        self.run_button = tk.Button(sweep_frame, text="Run", bg='#bfc6db', fg='#323338', font=widget_button_font,
                                    width=15, command=self.start_measure)
        self.run_button.grid(row=17, column=1, columnspan=3, pady=(30, 20), sticky=tk.W)

        self.visibility()

        sweep_frame["width"] = 4000
        sweep_frame["height"] = 4000

    def start_measure(self):
        # TODO ak nie je konektnuty tak nemoze kliknut na Run
        measure = True

        start = self.start_entry.get()
        stop = self.stop_entry.get()

        validation1 = data_validation.validate_float(self.start_entry.get())
        validation2 = data_validation.validate_float(self.stop_entry.get())

        if validation1 and validation2:
            if data_validation.validate_start_stop(start, stop):
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
                print("GUI start measure")
                self.run_measure()
            else:
                print("GUI stop measure")
                self.stop_measure()
        else:
            print("bad input")

    def stop_measure(self):
        print("GUI zastavujem meranie")
        self.run_button["text"] = "Run"
        self.gui.program.end_measurement()
        # self.gui.program.adapter.end_measurement()

    def run_measure(self):
        # TODO zmena stavu
        # TODO volanie všetkých metód

        # TODO set_settings...
        if self.autosave.get() == 1:
            self.gui.project.save()

        # TODO pred meranim prejde do stavu 3(podla dokumentu v testovacich scenaroch)
        if self.continuous.get() == 0:
            self.gui.program.queue_function("measure()")
            # print(self.gui.program.adapter.measure())
        else:
            self.run_button["text"] = "Stop"
            self.gui.program.queue_function("start_measurement()")
            # self.gui.program.adapter.start_measurement()

        # self.saveSettings()

    def visibility(self):
        if self.continuous.get() == 0:
            self.frame_entry.delete(0, tk.END)
            self.frame_entry.insert(tk.END, "0")
            self.frame_entry["state"] = tk.DISABLED
            self.max_label["text"] = "/ ---"
            self.frame_down_button["state"] = tk.DISABLED
            self.frame_up_button["state"] = tk.DISABLED
            self.frame_last_button["state"] = tk.DISABLED
        else:
            self.frame_entry.delete(0, tk.END)
            self.frame_entry.insert(tk.END, "0")  # TODO: aktuálny frame
            self.frame_entry["state"] = tk.NORMAL
            self.max_label["text"] = "/ 10"  # TODO: počet Frame
            self.frame_down_button["state"] = tk.NORMAL
            self.frame_up_button["state"] = tk.NORMAL
            self.frame_last_button["state"] = tk.NORMAL
