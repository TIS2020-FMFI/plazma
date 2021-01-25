import tkinter as tk

class Terminal:

    def __init__(self, parent):
        self.parent = parent
        self.list = []
        self.current = None

    def openNewWindow(self):
        newWindow = tk.Toplevel(self.parent.main_gui.window, bg="black")
        newWindow.bind("<Return>", lambda x: self.submit())
        newWindow.bind('<Up>', lambda x: self.up())
        newWindow.bind('<Down>', lambda x: self.down())

        newWindow.title("Terminal")
        newWindow.geometry("600x400")
        newWindow.minsize(250,100)

        self.text = tk.Text(newWindow, bg="black", fg="chartreuse")
        self.text.grid(row=0, column=0, columnspan=3, sticky="N"+"S"+"E"+"W")
        self.text.bind("<Key>", lambda e: "break")
        for i in self.list:
            self.text.insert(tk.END, "\n" + i)

        scrollbar = tk.Scrollbar(newWindow)
        self.text.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.text.yview)
        scrollbar.grid(column=3, row=0, sticky="N"+"S"+"W")

        commandLabel = tk.Label(newWindow, text="Command:", fg="white", bg="black")
        commandLabel.grid(row=1, sticky="E")

        self.command = tk.Entry(newWindow, bg="snow3", fg="black", bd=4)
        self.command.grid(row=1, column=1, sticky="N"+"S"+"E"+"W", padx=5)

        submit = tk.Button(newWindow, text="Submit", command=self.submit, bg="gray13", fg="white", bd=6, padx=5)
        submit.grid(row=1, column=2, columnspan=2, sticky="E"+"W")

        newWindow.grid_columnconfigure(0, weight=1, minsize=60)
        newWindow.grid_columnconfigure(1, weight=15, minsize=100)
        newWindow.grid_columnconfigure(2, weight=3, minsize=50)
        newWindow.grid_rowconfigure(0, weight=1)

    def submit(self):
        txt = self.command.get()
        if len(txt) < 1:
            return
        self.parent.main_gui.program.adapter.send(txt)
        self.list.append(txt)
        self.current = None
        self.text.insert(tk.END, "\n" + txt)
        self.command.delete(0, tk.END)

    def up(self):
        if not self.list:
            return
        elif self.current is None:
            self.current = len(self.list)-1
        else:
            self.current = (self.current-1) % len(self.list)
        self.command.delete(0, tk.END)
        self.command.insert(0, self.list[self.current])

    def down(self):
        if not self.list:
            return
        elif self.current is None:
            self.current = len(self.list)-1
        else:
            self.current = (self.current+1) % len(self.list)
        self.command.delete(0, tk.END)
        self.command.insert(0, self.list[self.current])