import tkinter as tk
import tkinter.font as tkFont
import data_validation


class CalibrationGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui

        widget_title_font = tkFont.Font(family="Tw Cen MT", size=self.main_gui.title_font_size, weight="bold")
        widget_button_font = tkFont.Font(family="Tw Cen MT", size=self.main_gui.label_font)
        widget_label_bold_font = tkFont.Font(family="Tw Cen MT", size=self.main_gui.label_font, weight="bold")
        widget_port_font = tkFont.Font(family="Tw Cen MT", size=self.main_gui.label_font_small)

        calibration_frame = tk.LabelFrame(self.main_gui.window, text="CALIBRATION",
                                          fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        calibration_frame.grid(row=0, rowspan=2, column=2, sticky=tk.N + tk.W, padx=(self.main_gui.padx, 0))

        self.save_calib_button = tk.Button(calibration_frame, text="SAVE CALIBRATION",
                                           font=widget_button_font, bg='#bfc6db', fg='#323338',
                                           command=self.save_calibration)
        self.save_calib_button.grid(row=0, column=0, pady=5, padx=5)
        self.save_calib_button["state"] = tk.DISABLED

        self.load_calib_button = tk.Button(calibration_frame, text="LOAD CALIBRATION",
                                           font=widget_button_font, bg='#bfc6db', fg='#323338',
                                           command=self.load_calibration)
        self.load_calib_button.grid(row=0, column=1, pady=5, padx=5)
        self.load_calib_button["state"] = tk.DISABLED

        line_label = tk.Label(calibration_frame,
                              text="_______________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        port_extension_label = tk.Label(calibration_frame, text="Port Extension bar",
                                        fg='#323338', bg="#f2f3fc", font=widget_label_bold_font)
        port_extension_label.grid(row=2, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        port1_length_label = tk.Label(calibration_frame, text="PORT1 Length/(μs):",
                                      fg='#323338', bg="#f2f3fc", font=widget_port_font)
        port1_length_label.grid(row=3, column=0, sticky=tk.W + tk.N, padx=5)

        self.port1 = tk.StringVar()
        self.port1_spin_box = tk.Spinbox(calibration_frame, textvariable=self.port1, from_=0, command=self.adjust,
                                         to=1000000000000, increment=1, justify=tk.RIGHT,
                                         font=widget_port_font)
        self.port1_spin_box.grid(row=3, column=1, sticky=tk.W, pady=3)
        self.port1_spin_box["state"] = tk.DISABLED
        self.port1.set(self.main_gui.program.settings.get_port1())

        port2_length_label = tk.Label(calibration_frame, text="PORT2 Length/(μs): ", fg='#323338', bg="#f2f3fc",
                                      font=widget_port_font)
        port2_length_label.grid(row=4, column=0, sticky=tk.W + tk.N, padx=5)

        self.port2 = tk.StringVar()
        self.port2_spin_box = tk.Spinbox(calibration_frame, textvariable=self.port2, from_=0, command=self.adjust,
                                         to=1000000000000, increment=1, justify=tk.RIGHT, font=widget_port_font)
        self.port2_spin_box.grid(row=4, column=1, sticky=tk.W, pady=3)
        self.port2_spin_box["state"] = tk.DISABLED
        self.port2.set(self.main_gui.program.settings.get_port2())

        velocity_factor_label = tk.Label(calibration_frame, text="Velocity Factor:",
                                         fg='#323338', bg="#f2f3fc", font=widget_port_font)
        velocity_factor_label.grid(row=5, column=0, sticky=tk.W + tk.N, padx=5, pady=5)

        self.vel_fact = tk.StringVar()
        self.vel_fact_spin_box = tk.Spinbox(calibration_frame, textvariable=self.vel_fact, command=self.adjust,
                                            from_=0.000, to=10.000, increment=0.001, format="%.3f",
                                            justify=tk.RIGHT, font=widget_port_font)
        self.vel_fact_spin_box.grid(row=5, column=1, sticky=tk.W, pady=3)
        self.vel_fact_spin_box["state"] = tk.DISABLED
        self.vel_fact.set(self.main_gui.program.settings.get_vel_factor())

        self.adjust_cal_button = tk.Button(calibration_frame, text="Adjust Calibration",
                                           font=widget_port_font, bg='#bfc6db')
        self.adjust_cal_button.grid(row=6, column=1, sticky=tk.W, pady=5)
        self.adjust_cal_button["command"] = self.adjust
        self.adjust_cal_button["state"] = tk.DISABLED

    def allow_calibration_load(self):
        self.load_calib_button["state"] = tk.NORMAL

    def disable_calibration_load(self):
        self.load_calib_button["state"] = tk.DISABLED

    def adjust(self):
        all_good = True
        vel_fact = self.vel_fact_spin_box.get()
        if data_validation.validate_velocity_factor(vel_fact):
            self.vel_fact_spin_box["fg"] = "black"
        else:
            self.vel_fact_spin_box["fg"] = "red"
            all_good = False

        port1 = self.port1_spin_box.get()
        if data_validation.validate_port_length(port1):
            self.port1_spin_box["fg"] = "black"
        else:
            self.port1_spin_box["fg"] = "red"
            all_good = False

        port2 = self.port2_spin_box.get()
        if data_validation.validate_port_length(port2):
            self.port2_spin_box["fg"] = "black"
        else:
            self.port2_spin_box["fg"] = "red"
            all_good = False

        if all_good:
            self.main_gui.program.queue_function(f"adjust_calibration({port1}, {port2}, {vel_fact})")

    def save_calibration(self):
        self.main_gui.program.queue_function("save_calib()")

    def load_calibration(self):
        self.main_gui.program.queue_function("load_calib()")

    def calibration_state_connected(self):
        self.save_calib_button["state"] = tk.NORMAL

        if self.main_gui.program.project.get_calibration() is None:
            self.load_calib_button["state"] = tk.DISABLED
        else:
            self.load_calib_button["state"] = tk.NORMAL

        self.adjust_cal_button["state"] = tk.NORMAL
        self.port1_spin_box["state"] = tk.NORMAL
        self.port2_spin_box["state"] = tk.NORMAL
        self.vel_fact_spin_box["state"] = tk.NORMAL

    def calibration_state_disconnected(self):
        self.save_calib_button["state"] = tk.DISABLED
        self.load_calib_button["state"] = tk.DISABLED
        self.adjust_cal_button["state"] = tk.DISABLED
        self.port1_spin_box["state"] = tk.DISABLED
        self.port2_spin_box["state"] = tk.DISABLED
        self.vel_fact_spin_box["state"] = tk.DISABLED

    def load_project_calibration(self):
        self.vel_fact.set(str(self.main_gui.program.settings.vel_factor))
        self.port1.set(str(self.main_gui.program.settings.port1))
        self.port2.set(str(self.main_gui.program.settings.port2))
