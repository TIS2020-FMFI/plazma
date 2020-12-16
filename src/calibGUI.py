import tkinter as tk
from tkinter import filedialog, ttk
import tkinter.font as tkFont
import data_validation


class CalibrationGui:
    def __init__(self, program):
        self.p = program
        self.create_calib_gui()

    def create_calib_gui(self):
        widget_title_font = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_button_font = tkFont.Font(family="Tw Cen MT", size=13)
        widget_label_bold_font = tkFont.Font(family="Tw Cen MT", size=14, weight="bold")
        widget_port_font = tkFont.Font(family="Tw Cen MT", size=11)

        calibration_frame = tk.LabelFrame(self.p.window, text="CALIBRATION",
                                          fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        calibration_frame.grid(row=0, rowspan=1, column=2, sticky=tk.N, padx=5)

        save_calib_button = tk.Button(calibration_frame, text="SAVE CALIBRATION",
                                      font=widget_button_font, bg='#bfc6db', fg='#323338',
                                      command=self.save_calibration)
        save_calib_button.grid(row=0, column=0, pady=5, padx=5)

        load_calib_button = tk.Button(calibration_frame, text="LOAD CALIBRATION",
                                      font=widget_button_font, bg='#bfc6db', fg='#323338',
                                      command=self.load_calibration)
        load_calib_button.grid(row=0, column=1, pady=5, padx=5)

        line_label = tk.Label(calibration_frame,
                              text="_______________________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        port_extension_label = tk.Label(calibration_frame, text="Port Extension bar",
                                        fg='#323338', bg="#f2f3fc", font=widget_label_bold_font)
        port_extension_label.grid(row=2, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        port1_length_label = tk.Label(calibration_frame, text="PORT1 Length/(m):",
                                      fg='#323338', bg="#f2f3fc", font=widget_port_font)
        port1_length_label.grid(row=3, column=0, sticky=tk.W + tk.N, padx=5)

        self.port1 = tk.StringVar()
        self.port1_spin_box = tk.Spinbox(calibration_frame, textvariable=self.port1, from_=0.0000,
                                         to=1.0000, increment=0.0001, format="%.4f", justify=tk.RIGHT,
                                         font=widget_port_font)
        self.port1_spin_box.grid(row=3, column=1, sticky=tk.W, pady=3)

        port2_length_label = tk.Label(calibration_frame, text="PORT2 Length/(m): ", fg='#323338', bg="#f2f3fc",
                                      font=widget_port_font)
        port2_length_label.grid(row=4, column=0, sticky=tk.W + tk.N, padx=5)

        self.port2 = tk.StringVar()
        self.port2_spin_box = tk.Spinbox(calibration_frame, textvariable=self.port2,
                                         from_=0.0000, to=1.0000, increment=0.0001, format="%.4f",
                                         justify=tk.RIGHT, font=widget_port_font)
        self.port2_spin_box.grid(row=4, column=1, sticky=tk.W, pady=3)

        velocity_factor_label = tk.Label(calibration_frame, text="Velocity Factor:",
                                         fg='#323338', bg="#f2f3fc", font=widget_port_font)
        velocity_factor_label.grid(row=5, column=0, sticky=tk.W + tk.N, padx=5, pady=5)

        self.vel_fact = tk.StringVar()
        self.vel_fact_spin_box = tk.Spinbox(calibration_frame, textvariable=self.vel_fact,
                                            from_=0.00, to=1.00, increment=0.01, format="%.2f",
                                            justify=tk.RIGHT, font=widget_port_font)
        self.vel_fact_spin_box.grid(row=5, column=1, sticky=tk.W, pady=3)

        self.adjust_cal_button = tk.Button(calibration_frame, text="Adjust Calibration",
                                           font=widget_port_font, bg='#bfc6db')
        self.adjust_cal_button.grid(row=6, column=1, sticky=tk.W, pady=5)
        self.adjust_cal_button["command"] = self.adjust_calibration

    def adjust_calibration(self):

        if data_validation.validate_velocity_factor(self.vel_fact_spin_box.get()):
            self.vel_fact_spin_box["fg"] = "black"
        else:
            self.vel_fact_spin_box["fg"] = "red"

        if data_validation.validate_port_length(self.port1_spin_box.get()):
            self.port1_spin_box["fg"] = "black"
        else:
            self.port1_spin_box["fg"] = "red"

        if data_validation.validate_port_length(self.port2_spin_box.get()):
            self.port2_spin_box["fg"] = "black"
        else:
            self.port2_spin_box["fg"] = "red"

    def save_calibration(self):
        print("Ukladám kalilbráciu do pamäte")
        self.p.program.project.set_calibration(self.p.program.adapter.get_calibration())

    def load_calibration(self):
        print("Načítavam kalibráciu")
        self.p.program.adapter.set_calibration(self.p.program.project.get_calibration())
        print(self.p.program.project.get_calibration())

    def adjust(self):
        # TODO
        pass
