import tkinter as tk
import tkinter.font as tk_font
import data_validation


class GpibGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui

        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.title_font_size, weight="bold")
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)
        widget_button_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font)

        gpib_frame = tk.LabelFrame(self.main_gui.window, text="GPIB", fg="#323338", bg='#f2f3fc',
                                   font=widget_title_font, relief=tk.RIDGE)
        gpib_frame.grid(row=0, column=1, sticky=tk.N+tk.W, pady=(0, 0))

        address_label = tk.Label(gpib_frame, text="Address:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        address_label.grid(row=0, column=1, sticky=tk.W, padx=20)

        self.address_entry = tk.Entry(gpib_frame, width=10)
        self.address_entry.insert(tk.END, self.main_gui.program.settings.get_address())
        self.address_entry["fg"] = "#2f3136"
        self.address_entry["font"] = widget_label_font
        self.address_entry.grid(row=0, column=1, sticky=tk.W, padx=(100, 0), pady=3)

        line_label = tk.Label(gpib_frame, text=self.main_gui.line_count * "_",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        self.connect_button = tk.Button(gpib_frame, text="  CONNECT  ", bg='#bfc6db', fg='#323338',
                                        font=widget_button_font)
        self.connect_button["command"] = self.start_connect
        self.connect_button.grid(row=2, column=1, padx=(100, 0), pady=(5, 0))

        line_label = tk.Label(gpib_frame, text=self.main_gui.line_count * "_",
                              fg="#b3b3b5", bg="#f2f3fc")
        line_label.grid(row=3, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        gpib_label = tk.Label(gpib_frame, text="GPIB terminal:", fg="#323338", bg='#f2f3fc', font=widget_label_font)
        gpib_label.grid(row=4, column=1, sticky=tk.W, pady=10, padx=20)

        self.gpib_button = tk.Button(gpib_frame, text="OPEN", bg='#bfc6db', fg='#323338',
                                     command=self.open_gpib_terminal)
        self.gpib_button['font'] = widget_button_font
        self.gpib_button.grid(row=4, column=2, padx=(10, 20), pady=self.main_gui.sweep_pady)
        self.gpib_button["state"] = tk.DISABLED

    def gpib_state_disconnected(self):
        self.address_entry["state"] = tk.NORMAL
        self.gpib_button["state"] = tk.DISABLED

    def gpib_state_connected(self):
        self.address_entry["state"] = tk.DISABLED
        self.gpib_button["state"] = tk.NORMAL

    def open_gpib_terminal(self):
        self.main_gui.program.queue_function("open_terminal()")

    def start_connect(self):
        if self.connect_button["text"] == "  CONNECT  ":
            if data_validation.validate_address(self.address_entry.get()):
                self.address_entry["bg"] = "white"

                self.main_gui.program.queue_function(f"connect({self.address_entry.get()})")
            else:
                self.address_entry["bg"] = "#d44242"
        else:
            self.main_gui.program.queue_function("disconnect()")

    def update_button_connected(self):
        self.connect_button['text'] = "DISCONNECT"
        self.main_gui.state_connected()

    def update_button_disconnected(self):
        self.connect_button["text"] = "  CONNECT  "
        self.main_gui.state_disconnected()

    def load_project_settings(self):
        self.address_entry.delete(0, tk.END)
        self.address_entry.insert(tk.END, self.main_gui.program.settings.get_address())
