import tkinter as tk
from tkinter import filedialog
import tkinter.font as tk_font


class ProjectGui:
    def __init__(self, program):
        self.p = program
        self.create_project_gui()

    def create_project_gui(self):
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=13)
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=13)

        project_frame = tk.LabelFrame(self.p.window, text="PROJECT", fg="#323338", bg='#f2f3fc',
                                      font=widget_title_font, relief=tk.RIDGE)
        project_frame.grid(row=1, column=1, sticky=tk.N, pady=(0, 0))

        self.project_name_entry = tk.Entry(project_frame, width=30, fg="#b3b3b5", font=widget_label_font)
        self.project_name_entry.grid(row=0, column=0, columnspan=4, sticky=tk.W, padx=10, pady=5)
        self.project_name_entry.insert(tk.E, "Project Name")

        self.project_descrip_text = tk.Text(project_frame, height=5, width=30, fg="#b3b3b5", font=widget_label_font)
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
        name = self.project_name_entry.get()
        description = self.project_descrip_text.get(1.0, tk.END)

        self.p.program.file_manager.save_project(path, name, description)

    def load(self):
        # TODO: load project

        path = tk.filedialog.askdirectory()
        self.p.program.file_manager.load_project(path)

        #  po loade nastavit hodnoty v polickach z self.p.program.file_manager.get_settings() teda z settings.txt
