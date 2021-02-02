import tkinter as tk
import tkinter.font as tk_font


class InstStateGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui

        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.title_font_size, weight="bold")
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)

        inst_state_frame = tk.LabelFrame(self.main_gui.window, text="INST. STATE", fg="#323338",
                                         bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        inst_state_frame.grid(row=1, column=1, pady=(self.main_gui.pady_1, 0), sticky=tk.N + tk.W)

        self.preset_button = tk.Button(inst_state_frame, text="PRESET", bg='#bfc6db', fg='#323338',
                                       font=widget_button_font, command=self.preset)
        self.preset_button.grid(row=0, column=1, padx=10, pady=(10, 5))
        self.preset_button["state"] = tk.DISABLED

        line_label = tk.Label(inst_state_frame, text=self.main_gui.line_count * "_", fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        self.save_state_button = tk.Button(inst_state_frame, text="SAVE STATE", bg='#bfc6db',
                                           fg='#323338', font=widget_button_font, command=self.save_state)
        self.save_state_button.grid(row=4, column=1, columnspan=1, pady=(10, 5))
        self.save_state_button["state"] = tk.DISABLED

        self.recall_state_button = tk.Button(inst_state_frame, text="RECALL STATE", bg='#bfc6db',
                                             fg='#323338', font=widget_button_font, command=self.recall_state)
        self.recall_state_button.grid(row=5, column=1, columnspan=1, pady=(0, 10))
        self.recall_state_button["state"] = tk.DISABLED

    def preset(self):
        self.main_gui.program.queue_function("preset()")

    def allow_recall_state(self):
        self.recall_state_button["state"] = tk.NORMAL

    def disable_recall_state(self):
        self.recall_state_button["state"] = tk.DISABLED

    def instrumentstate_state_disconnected(self):
        self.preset_button["state"] = tk.DISABLED
        self.save_state_button["state"] = tk.DISABLED
        self.recall_state_button["state"] = tk.DISABLED

    def instrumentstate_state_connected(self):
        self.preset_button["state"] = tk.NORMAL
        self.save_state_button["state"] = tk.NORMAL
        if self.main_gui.program.project.get_state() is None:
            self.recall_state_button["state"] = tk.DISABLED
        else:
            self.recall_state_button["state"] = tk.NORMAL

    def save_state(self):
        print("GUI:ukladám stav prístroja do pamäte")
        self.main_gui.program.queue_function("save_state()")

    def recall_state(self):
        print("GUI:načítavam stav z pamäte do prístroja")
        self.main_gui.program.queue_function("recall_state()")


