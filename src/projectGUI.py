import os
import tkinter as tk
from tkinter import filedialog
import tkinter.font as tk_font


class ProjectGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui

        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.title_font_size, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)

        project_frame = tk.LabelFrame(self.main_gui.window, text="PROJECT", fg="#323338", bg='#f2f3fc',
                                      font=widget_title_font, relief=tk.RIDGE)
        project_frame.grid(row=2, column=1, sticky=tk.N + tk.W, pady=(self.main_gui.pady_1, 0))

        self.project_name_entry = tk.Entry(project_frame, width=self.main_gui.project_weight, fg="#323338",
                                           font=widget_label_font)
        self.project_name_entry.grid(row=0, column=0, columnspan=4, sticky=tk.W, padx=12, pady=5)
        self.project_name_entry.insert(tk.E, "Project Name")

        self.project_descrip_text = tk.Text(project_frame, height=5, width=self.main_gui.project_weight,
                                            fg="#323338", font=widget_label_font)
        self.project_descrip_text.grid(row=1, column=0, columnspan=4, padx=10, pady=10)
        self.project_descrip_text.insert(tk.END, "Enter project description")

        save_project_button = tk.Button(project_frame, text="SAVE", bg='#bfc6db', fg='#323338',
                                        command=self.save, font=widget_button_font)
        save_project_button.grid(row=2, column=1, pady=10)

        load_project_button = tk.Button(project_frame, text="LOAD", bg='#bfc6db', fg='#323338',
                                        command=self.load, font=widget_button_font)
        load_project_button.grid(row=2, column=2, pady=10)

    def save(self):
        path = tk.filedialog.askdirectory()
        if path == "":
            return False
        name = self.project_name_entry.get()
        description = self.project_descrip_text.get(1.0, tk.END)
        if not self.main_gui.program.project.exists_data():
            self.main_gui.sweep.send_settings()
        self.main_gui.program.file_manager.save_project(path, name, description)
        tk.messagebox.showinfo(title="Project", message="Project successfully saved!")
        return True

    def load(self):
        path = tk.filedialog.askdirectory()
        if path == "":
            return
        self.main_gui.program.file_manager.load_project(path)
        self.main_gui.gpib.load_project_settings()
        self.main_gui.info.change_state_label()
        self.project_name_entry.delete(0, tk.END)
        name = os.path.basename(path)
        self.project_name_entry.insert(tk.END, name)
        self.project_descrip_text.delete(1.0, tk.END)
        self.project_descrip_text.insert(tk.END, self.main_gui.program.file_manager.description)

        self.main_gui.calibration.load_project_calibration()
        self.main_gui.info.change_calibration_label()

        self.main_gui.sweep.load_project_sweep()
        tk.messagebox.showinfo(title="Project", message="Project successfully loaded!")


class InfoGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.title_font_size, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)

        info_frame = tk.LabelFrame(self.main_gui.window, text="INFO", fg="#323338", bg='#f2f3fc',
                                   font=widget_title_font, relief=tk.RIDGE, width=self.main_gui.info_weight,
                                   height=self.main_gui.info_height)
        info_frame.grid(row=3, column=1, sticky=tk.N + tk.W, pady=(self.main_gui.pady_1, 0))

        self.connect_info_label = tk.Label(info_frame, text=" • Disconnected ", fg="#d44242", bg='#f2f3fc',
                                           font=widget_label_font)
        self.connect_info_label.place(x=10, y=10)

        self.device_label = tk.Label(info_frame, text="from device", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.device_label.place(x=120, y=10)

        self.calibration_label = tk.Label(info_frame, text="Calibration", fg="#323338", bg='#f2f3fc',
                                          font=widget_label_font)
        self.calibration_label.place(x=15, y=50)

        self.calib_load_label = tk.Label(info_frame, text="NULL", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.calib_load_label.place(x=170, y=50)

        self.calibration_type_label = tk.Label(info_frame, text="Calibration type", fg="#323338", bg='#f2f3fc',
                                               font=widget_label_font)
        self.calibration_type_label.place(x=15, y=70)

        self.calib_type_load_label = tk.Label(info_frame, text="NULL", fg="#323338", bg='#f2f3fc',
                                              font=widget_label_font)
        self.calib_type_load_label.place(x=170, y=70)

        self.state_label = tk.Label(info_frame, text="State", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.state_label.place(x=15, y=110-self.main_gui.minus)

        self.state_load_label = tk.Label(info_frame, text="NULL", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.state_load_label.place(x=170, y=110-self.main_gui.minus)

        self.measurements_label = tk.Label(info_frame, text="Data", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.measurements_label.place(x=15, y=150-self.main_gui.minus)

        self.measurements_data = tk.Label(info_frame, text="NULL", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        self.measurements_data.place(x=170, y=150-self.main_gui.minus)

    def change_connect_label(self):
        if self.connect_info_label["text"] == " • Disconnected ":
            self.connect_info_label["text"] = " • Connected "
            self.connect_info_label["fg"] = "green"
            self.device_label["text"] = "to device"
            self.device_label.place(x=105, y=10)
        else:
            self.connect_info_label["text"] = " • Disconnected "
            self.connect_info_label["fg"] = "#d44242"
            self.device_label["text"] = "from device"
            self.device_label.place(x=120, y=10)

    def change_state_label(self):
        if self.main_gui.program.project.get_state() is not None:
            self.state_load_label["text"] = "SAVED"
            if self.main_gui.program.adapter.connected:
                self.main_gui.state.allow_recall_state()
        else:
            self.state_load_label["text"] = "NULL"
            self.main_gui.state.disable_recall_state()

    def change_calibration_label(self):
        if self.main_gui.program.project.get_calibration() is not None:
            self.calib_load_label["text"] = "SAVED"
            self.calib_type_load_label["text"] = str(self.main_gui.program.project.get_calibration_type())
            if self.main_gui.program.adapter.connected:
                self.main_gui.calibration.allow_calibration_load()
        else:
            self.calib_load_label["text"] = "NULL"
            self.calib_type_load_label["text"] = "NULL"
            self.main_gui.calibration.disable_calibration_load()

    def change_data_label(self):
        if self.main_gui.program.project.exists_data():
            self.measurements_data["text"] = "measured"
        else:
            self.measurements_data["text"] = "NULL"

    def waiting_data_label(self):
        self.measurements_data["text"] = "wait please..."
        
    def waiting_calibration_label(self):
        self.calib_load_label["text"] = "wait please..."
        
