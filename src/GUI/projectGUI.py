import os
import tkinter as tk
from tkinter import filedialog
import tkinter.font as tkFont
from datetime import datetime

class projectGUI:
    def __init__(self, program):
        self.p = program
        self.createProjectGUI()

    def createProjectGUI(self):
        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetLabelFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetButtonFont = tkFont.Font(family="Tw Cen MT", size=13)

        projectFrame = tk.LabelFrame(self.p.window, text="PROJECT", fg="#323338", bg='#f2f3fc', font=widgetTitlefont, relief=tk.RIDGE)
        projectFrame.grid(row=1, column=1, sticky=tk.N, pady=(5, 0))

        self.projectNameEntry = tk.Entry(projectFrame, width=30,fg = "#b3b3b5", font = widgetLabelFont)
        self.projectNameEntry.grid(row=0, column=0, columnspan=4, sticky=tk.W, padx=10, pady=5)
        self.projectNameEntry.insert(tk.E, "Project Name")



        self.projectDescripText = tk.Text(projectFrame, height=5, width=30, fg="#b3b3b5", font=widgetLabelFont)
        self.projectDescripText.grid(row=1, column=0, columnspan=4, padx=10, pady=10)
        self.projectDescripText.insert(tk.END, "Enter project description")



        saveProjectButton = tk.Button(projectFrame, text="SAVE", bg='#bfc6db', fg='#323338', command= self.save, font = widgetButtonFont)
        saveProjectButton.grid(row=2, column=1, pady=10)

        loadProjectButton = tk.Button(projectFrame, text="LOAD", bg='#bfc6db', fg='#323338', command=self.load, font=widgetButtonFont)
        loadProjectButton.grid(row=2, column=2, pady=10)

    def save(self):
        # TO DO: save project

        folderName = tk.filedialog.askdirectory()
        if(self.projectNameEntry.get() == "Project Name" or self.projectNameEntry.get() == ""):
            now = datetime.now()
            date = datetime.date(now)

            fileName = "PROJECT - " + str(date)
            path = os.path.join(folderName, fileName)
            os.mkdir(path)
        else:
            fileName = self.projectNameEntry.get()
            path = os.path.join(folderName,fileName)
            os.mkdir(path)

        self.saveDescription(path)

    def saveDescription(self,path):

        if (self.projectDescripText.get(1.0,tk.END) != "Project Name" and self.projectDescripText.get(1.0,tk.END) != ""):
            name = "description.txt"
            filePath = os.path.join(path, name)
            f = open(filePath, "w")
            f.write(self.projectDescripText.get(1.0, tk.END))
            f.close()

    def load(self):
        #TO DO: load project

        folderName = tk.filedialog.askdirectory()