import tkinter as tk
import tkinter.font as tkFont
import data_validation


class GpibGui:
    def __init__(self, program):
        self.p = program
        self.create_gpib_gui()

    def create_gpib_gui(self):

        widget_title_font = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widget_label_font = tkFont.Font(family="Tw Cen MT", size=13)
        widget_button_font = tkFont.Font(family="Tw Cen MT", size=13)

        gpib_frame = tk.LabelFrame(self.p.window, text="GPIB", fg="#323338", bg='#f2f3fc',
                                   font=widget_title_font, relief=tk.RIDGE)
        gpib_frame.grid(row=0, column=1, sticky=tk.N)

        address_label = tk.Label(gpib_frame, text="Address:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        address_label.grid(row=0, column=1, sticky=tk.W, padx=20)

        self.address_entry = tk.Entry(gpib_frame, width=10)
        self.address_entry.insert(tk.END, "16")
        self.address_entry["fg"] = "#2f3136"
        self.address_entry["font"] = widget_label_font
        self.address_entry.grid(row=0, column=1, sticky=tk.W, padx=(100,0), pady=3)

        line_label = tk.Label(gpib_frame, text="_____________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        connect_label = tk.Label(gpib_frame, text="Connect:", fg="#323338", bg="#f2f3fc", font=widget_label_font)
        connect_label.grid(row=2, column=1, sticky=tk.W + tk.N, padx=10, pady=10)

        self.connect_button = tk.Button(gpib_frame, text="ON", bg="#5bb38a", fg='#323338', font=widget_button_font)
        self.connect_button["command"] = self.start_connect
        self.connect_button.grid(row=2, column=2, padx=10)

        line_label = tk.Label(gpib_frame, text="_____________________________________________________",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=3, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        gpib_label = tk.Label(gpib_frame, text="GPIB terminal:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        gpib_label.grid(row=4, column=1, sticky=tk.W, pady=10, padx=20)

        self.gpib_button = tk.Button(gpib_frame, text="OPEN", bg='#bfc6db', fg='#323338',
                                     command=self.open_gpib_terminal)
        self.gpib_button['font'] = widget_button_font
        self.gpib_button.grid(row=4, column=2, padx=(10,20), pady=15)

    def disable(self):
        self.address_entry["state"] = tk.DISABLED

    def enable(self):
        self.address_entry["state"] = tk.NORMAL

    def open_gpib_terminal(self):
        # TODO: otvorít GPIB terminál

        print("Otváram GPIB terminál.")

    def start_connect(self):
        if self.connect_button["text"] == "ON":
            if data_validation.validate_address(self.address_entry.get()):
                self.address_entry["bg"] = "white"

                print("Pripájam sa na adresu: ", self.address_entry.get())
                self.p.program.adapter.connect(self.address_entry.get())
            else:
                print("oprav adresu, zly format")
                self.address_entry["bg"] = "#d44242"
        else:
            self.p.program.adapter.disconnect()

    def update_button_connected(self):
        print("Pripájam prístroj")
        self.connect_button['text'] = "OFF"
        self.connect_button["bg"] = "#b5555a"
        self.disable()

    def update_button_disconnected(self):       # toto sa bude spustat threadom adapteru
        print("Odpájam prístroj")
        self.connect_button["text"] = "ON"
        self.connect_button["bg"] = "#5bb38a"
        self.enable()
