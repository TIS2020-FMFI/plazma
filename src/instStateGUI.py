import tkinter as tk
import tkinter.font as tk_font
import threading


class InstStateGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui
    #     self.create_inst_state_gui()
    #
    # def create_inst_state_gui(self):
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=13)

        inst_state_frame = tk.LabelFrame(self.main_gui.window, text="INST. STATE", fg="#323338",
                                         bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        inst_state_frame.grid(row=0, column=1, pady=(220, 10))

        self.preset_button = tk.Button(inst_state_frame, text="PRESET", bg='#bfc6db', fg='#323338',
                                       font=widget_button_font, command=self.preset)
        self.preset_button.grid(row=0, column=1, padx=10, pady=(10, 5))
        self.preset_button["state"] = tk.DISABLED

        line_label = tk.Label(inst_state_frame, text="_____________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
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
        # self.main_gui.program.adapter.preset()
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=3)

        root = tk.Tk()
        root.config(bg="white")
        root.geometry('230x50+200+500')
        preset_frame = tk.LabelFrame(root, width=230, height=50,  fg="#323338", bg="#91b8a0",
                                     relief=tk.RIDGE)
        preset_frame.place(x=0, y=0)

        reset = tk.Label(preset_frame, text="RESET SUCCESSFUL", fg='#323338', bg="#91b8a0")
        reset["font"] = widget_title_font
        reset.place(x=5, y=7)
        root.overrideredirect(1)

        threading.Timer(1.0, root.destroy).start()

        root.mainloop()

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
        # self.main_gui.program.project.set_state(self.main_gui.program.adapter.get_state())
        self.main_gui.program.queue_function("save_state()")

    def recall_state(self):
        # TO DO: Načítať uložený stav do prístroja
        print("GUI:načítavam stav z pamäte do prístroja")
        self.main_gui.program.queue_function("recall_state()")
        # self.main_gui.program.adapter.set_state(self.main_gui.program.project.get_state())

    def load_project_state(self):
        pass
        # TODO zistiť či je connect a či uložený stav, ak áno umožniť recall state tlačídlo
