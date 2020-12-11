import tkinter as tk
from tkinter import ttk, font
from tkinter import filedialog
import tkinter.font as tkFont

class graphsGUI:
    def __init__(self, program):
        self.p = program
        self.createGraphsGUI()
    def createGraphsGUI(self):

        widgetLabelFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetPortFont = tkFont.Font(family="Tw Cen MT", size=11)

        graphFrame = tk.LabelFrame(self.p.window, text="GRAPHS", fg="#323338", bg='#f2f3fc', font=widgetTitlefont,relief=tk.RIDGE)
        graphFrame.grid(row=0, column=3, sticky=tk.NW)

        label = tk.Label(graphFrame, text="Tu budú grafy", fg='#323338', bg="#f2f3fc")
        label["font"] = widgetLabelFont
        label.grid(row=0, column=0, columnspan=4)
