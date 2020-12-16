import tkinter as tk
import tkinter.font as tkFont


class InstStateGui:
    def __init__(self, program):
        self.p = program
        self.create_inst_state_gui()

    def create_inst_state_gui(self):
        widget_title_font = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_button_font = tkFont.Font(family="Tw Cen MT", size=13)

        inst_state_frame = tk.LabelFrame(self.p.window, text="INST. STATE", fg="#323338",
                                         bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        inst_state_frame.grid(row=0, column=1, pady=(208, 0))

        self.preset_button = tk.Button(inst_state_frame, text="PRESET", bg='#bfc6db', fg='#323338',
                                       font=widget_button_font, command=self.p.program.adapter.preset)
        self.preset_button.grid(row=0, column=1, padx=10, pady=(10,5))

        line_label = tk.Label(inst_state_frame, text="_____________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        self.save_state_button = tk.Button(inst_state_frame, text="SAVE STATE", bg='#bfc6db',
                                           fg='#323338', font=widget_button_font, command=self.save_state)
        self.save_state_button.grid(row=4, column=1, columnspan=1, pady=(10,5))

        self.recall_state_button = tk.Button(inst_state_frame, text="RECALL STATE", bg='#bfc6db',
                                             fg='#323338', font=widget_button_font, command=self.recall_state)
        self.recall_state_button.grid(row=5, column=1, columnspan=1, pady=(0,10))

    def disable(self):
        self.preset_button["state"] = tk.DISABLED
        self.save_state_button["state"] = tk.DISABLED
        self.recall_state_button["state"] = tk.DISABLED

    def enable(self):
        self.preset_button["state"] = tk.NORMAL
        self.save_state_button["state"] = tk.NORMAL
        self.recall_state_button["state"] = tk.NORMAL

    def save_state(self):
        print("ukladám stav prístroja do pamäte")
        self.p.program.project.set_state(self.p.program.adapter.get_state())

    def recall_state(self):
        # TO DO: Načítať uložený stav do prístroja
        print("načítavam stav z pamäte do prístroja")
        self.p.program.adapter.set_state(self.p.program.project.get_state())
        print("Posielas stav: \n" + self.p.program.project.get_state())
