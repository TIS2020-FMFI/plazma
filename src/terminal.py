import tkinter as tk


class Terminal:
    def __init__(self, parent):
        self.parent = parent
        self.list = []
        self.current = None
        self.text = None
        self.command = None
        self.window = None
        self.all = []

    def is_open(self):
        return self.window is not None

    def open_new_window(self):
        new_window = tk.Toplevel(self.parent.gui.window, bg="black")
        new_window.grab_set()
        new_window.bind("<Return>", lambda x: self.submit())
        new_window.bind('<Up>', lambda x: self.up())
        new_window.bind('<Down>', lambda x: self.down())

        new_window.title("Terminal")
        new_window.geometry("600x400")
        new_window.minsize(250, 100)

        self.text = tk.Text(new_window, bg="black", fg="chartreuse")
        self.text.tag_config('message', foreground="yellow")
        self.text.tag_config('command', foreground="chartreuse")
        self.text.grid(row=0, column=0, columnspan=3, sticky="N" + "S" + "E" + "W")
        self.text.bind("<Key>", lambda e: "break")

        scrollbar = tk.Scrollbar(new_window)
        self.text.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.text.yview)
        scrollbar.grid(column=3, row=0, sticky="N" + "S" + "W")

        command_label = tk.Label(new_window, text="Command:", fg="white", bg="black")
        command_label.grid(row=1, sticky="E")

        self.command = tk.Entry(new_window, bg="snow3", fg="black", bd=4)
        self.command.grid(row=1, column=1, sticky="N" + "S" + "E" + "W", padx=5)
        self.command.focus_set()

        submit = tk.Button(new_window, text="Submit", command=self.submit, bg="gray13", fg="white", bd=6, padx=5)
        submit.grid(row=1, column=2, columnspan=2, sticky="E" + "W")

        new_window.grid_columnconfigure(0, weight=1, minsize=60)
        new_window.grid_columnconfigure(1, weight=15, minsize=100)
        new_window.grid_columnconfigure(2, weight=3, minsize=50)
        new_window.grid_rowconfigure(0, weight=1)

        new_window.protocol("WM_DELETE_WINDOW", self.close_window)
        self.window = new_window

        for i in self.all:
            self.text.insert(tk.END, "\n" + i[0], i[1])
        self.text.see(tk.END)

    def close_window(self):
        self.window.grab_release()  # to return to normal
        self.window.destroy()
        self.window = None
        self.parent.queue_function("close_terminal()")

    def submit(self):
        txt = self.command.get().strip()
        if len(txt) < 1:
            return
        if txt.lower() == "exit":
            self.close_window()
            return
        if txt.lower() == "clear":
            self.all = []
            self.list = []
            self.text.delete(1.0, tk.END)
            self.command.delete(0, tk.END)
            return

        self.list.append(txt)
        self.all.append(('\u25B6' + txt, 'command'))
        self.current = None
        self.text.insert(tk.END, "\n\u25B6" + txt)
        self.text.see(tk.END)
        self.command.delete(0, tk.END)
        if txt.lower() == "help":
            self.print_message("""\t    s str  ... send the string using gpib_puts()
            q str  ... send a query and read a string
                       response using gpib_query()
            a      ... retrieve response with gpib_read_ASC()
            c      ... continuous gpib_read_ASC() until next input
            d n    ... continuous gpib_read_ASC() N-times
            b      ... retrieve response with gpib_read_BIN()
            ?      ... read and print status
            HELP   ... print this help
            EXIT   ... close terminal""")
            return
        self.parent.queue_function(f"terminal_send('{txt}')")

    def print_message(self, message):
        self.all.append((message, 'message'))
        self.text.insert(tk.END, "\n" + message, 'message')
        self.text.see(tk.END)
        self.command.delete(0, tk.END)

    def up(self):
        if not self.list:
            return
        elif self.current is None:
            self.current = len(self.list) - 1
        else:
            self.current = (self.current - 1) % len(self.list)
        self.command.delete(0, tk.END)
        self.command.insert(0, self.list[self.current])

    def down(self):
        if not self.list:
            return
        elif self.current is None:
            self.current = len(self.list) - 1
        else:
            self.current = (self.current + 1) % len(self.list)
        self.command.delete(0, tk.END)
        self.command.insert(0, self.list[self.current])
