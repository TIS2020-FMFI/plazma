import tkinter as tk
import tkinter.font as tkFont

class SweepGUI:
    def __init__(self, program):
        self.p = program
        self.createSweepGUI()

    def createSweepGUI(self):
        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetLabelFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetPortFont = tkFont.Font(family="Tw Cen MT", size=11)
        widgetButtonFont = tkFont.Font(family="Tw Cen MT", size=13)

        sweepFrame = tk.LabelFrame(self.p.window, text="SWEEP", fg="#323338", bg='#f2f3fc', font=widgetTitlefont, relief=tk.RIDGE)
        sweepFrame.grid(row=0, column=2, rowspan=8, sticky=tk.NW, padx=25, pady=(255, 0))

        freqMeasurelabel = tk.Label(sweepFrame, text="Frequency Measure", fg='#323338',
                                    bg="#f2f3fc")
        freqMeasurelabel["font"] = widgetLabelFont
        freqMeasurelabel.grid(row=0, column=0, columnspan=4, padx=(50, 50))

        self.freqVariable = tk.IntVar()
        self.freqMHzradiobutton = tk.Radiobutton(sweepFrame, text="MHz", bg='#f2f3fc', fg="#323338",
                                            variable=self.freqVariable, value=0, font=widgetLabelFont)
        self.freqGHzradiobutton = tk.Radiobutton(sweepFrame, text="GHz", bg='#f2f3fc', fg="#323338",
                                            variable=self.freqVariable, value=1, font=widgetLabelFont)

        self.freqMHzradiobutton.grid(row=1, column=0, columnspan=2, sticky=tk.W, pady=(10, 0), padx=20)
        self.freqGHzradiobutton.grid(row=1, column=2, columnspan=2, sticky=tk.E, pady=(10, 0), padx=20)

        self.freqMHzradiobutton.select()
        self.freqGHzradiobutton.deselect()
        self.freqVariable.set(0)

        lineLabel = tk.Label(sweepFrame, text="______________________________________________________",
                             fg="#b3b3b5",
                             bg="#f2f3fc")
        lineLabel.grid(row=2, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        startLabel = tk.Label(sweepFrame, text="Start:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        startLabel.grid(row=3, column=0, sticky=tk.W, padx=(20, 0), pady=(15, 0))

        self.startEntry = tk.Entry(sweepFrame, width=5)
        self.startEntry.grid(row=3, column=1, sticky=tk.W, pady=(15, 0))
        self.startEntry.insert(tk.END, "0")
        self.startEntry["font"] = widgetLabelFont

        stopLabel = tk.Label(sweepFrame, text="Stop:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        stopLabel.grid(row=3, column=2, sticky=tk.W, pady=(15, 0))

        self.stopEntry = tk.Entry(sweepFrame, width=5)
        self.stopEntry.grid(row=3, column=3, sticky=tk.W, padx=(0, 20), pady=(15, 0))
        self.stopEntry.insert(tk.END, "100")
        self.stopEntry["font"] = widgetLabelFont

        pointsLabel = tk.Label(sweepFrame, text="Points:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        pointsLabel.grid(row=4, column=0, sticky=tk.W, padx=(20, 0), pady=(5, 0))

        self.pointsEntry = tk.Entry(sweepFrame, width=5)
        self.pointsEntry.grid(row=4, column=1, sticky=tk.W, pady=(5, 0))
        self.pointsEntry.insert(tk.END, "200")
        self.pointsEntry["font"] = widgetLabelFont

        pointsLabel = tk.Label(sweepFrame, text="(/1601)", fg="#323338", bg='#f2f3fc', font=widgetPortFont)
        pointsLabel.grid(row=4, column=2, sticky=tk.W, pady=(5, 0))

        lineLabel = tk.Label(sweepFrame, text="______________________________________________________",
                             fg="#b3b3b5",
                             bg="#f2f3fc")
        lineLabel.grid(row=5, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        sParamLabel = tk.Label(sweepFrame, text="S - parameters",fg='#323338', bg="#f2f3fc")
        sParamLabel["font"] = widgetLabelFont
        sParamLabel.grid(row=6, column=0, columnspan=4, padx=(50, 50))

        self.s11 = tk.IntVar()
        self.s11Checkbutton = tk.Checkbutton(sweepFrame, variable=self.s11, text="S11", font=widgetPortFont, bg='#f2f3fc')
        self.s11Checkbutton.grid(row=7, column=1, sticky=tk.W, pady=(5, 0))

        self.s12 = tk.IntVar()
        self.s12Checkbutton = tk.Checkbutton(sweepFrame, variable=self.s12, text="S12", font=widgetPortFont,bg='#f2f3fc')
        self.s12Checkbutton.grid(row=7, column=2, sticky=tk.W, pady=(5, 0))

        self.s21 = tk.IntVar()
        self.s21Checkbutton = tk.Checkbutton(sweepFrame, variable=self.s21,text="S21", font=widgetPortFont, bg='#f2f3fc')
        self.s21Checkbutton.grid(row=8, column=1, sticky=tk.W, pady=0)

        self.s22 = tk.IntVar()
        self.s22Checkbutton = tk.Checkbutton(sweepFrame, variable=self.s22, text="S22", font=widgetPortFont, bg='#f2f3fc')
        self.s22Checkbutton.grid(row=8, column=2, sticky=tk.W, pady=0)

        lineLabel = tk.Label(sweepFrame, text="______________________________________________________", fg="#b3b3b5", bg="#f2f3fc")
        lineLabel.grid(row=9, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.measureVariable = tk.IntVar()
        self.MAradiobutton = tk.Radiobutton(sweepFrame, text="magnitude-angle", bg='#f2f3fc', fg="#323338",
                                       variable=self.measureVariable, value=0, font=widgetLabelFont)
        self.dbAradiobutton = tk.Radiobutton(sweepFrame, text="db-angle", bg='#f2f3fc', fg="#323338",
                                        variable=self.measureVariable, value=1, font=widgetLabelFont)
        self.RIAradiobutton = tk.Radiobutton(sweepFrame, text="real-imaginary", bg='#f2f3fc', fg="#323338",
                                        variable=self.measureVariable, value=2, font=widgetLabelFont)


        self.MAradiobutton.grid(row=10, column=0, columnspan=4, sticky=tk.W, pady=(10, 0), padx=20)
        self.dbAradiobutton.grid(row=11, column=0, columnspan=4, sticky=tk.W, pady=(0, 0), padx=20)
        self.RIAradiobutton.grid(row=12, column=0, columnspan=4, sticky=tk.W, pady=(0, 0), padx=20)
        self.MAradiobutton.select()
        self.measureVariable.set(0)
        self.dbAradiobutton.deselect()
        self.RIAradiobutton.deselect()

        lineLabel = tk.Label(sweepFrame, text="______________________________________________________",
                             fg="#b3b3b5",
                             bg="#f2f3fc")
        lineLabel.grid(row=13, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.continuous = tk.IntVar()
        self.continuousCheckbutton = tk.Checkbutton(sweepFrame, variable = self.continuous, text="continuous", font=widgetPortFont, bg='#f2f3fc')
        self.continuousCheckbutton.grid(row=14, column=0, columnspan=2, sticky=tk.W, pady=(5, 0), padx=(30,0))
        self.continuousCheckbutton["command"] = self.visibility

        self.autosave = tk.IntVar()
        self.autosaveCheckbutton = tk.Checkbutton(sweepFrame, variable=self.autosave,text="autosave", font=widgetPortFont, bg='#f2f3fc')
        self.autosaveCheckbutton.grid(row=14, column=2, columnspan=2,sticky=tk.W, pady=(5, 0))

        lineLabel = tk.Label(sweepFrame, text="______________________________________________________",
                             fg="#b3b3b5",
                             bg="#f2f3fc")
        lineLabel.grid(row=15, column=0, columnspan=4, sticky=tk.W + tk.N, padx=5, pady=0)

        self.frameLabel = tk.Label(sweepFrame, text="Frame:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        self.frameLabel.grid(row=16, column=0, sticky=tk.W, pady=(2, 0), padx=(15,0))

        self.frameEntry = tk.Entry(sweepFrame, width=3, fg= "#838691", font = widgetLabelFont)
        self.frameEntry.grid(row=16, column=1, sticky=tk.W, pady=(10,10))
        self.frameEntry.insert(tk.END, "1")

        self.maxLabel = tk.Label(sweepFrame, text="/10", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        self.maxLabel.grid(row=16, column=1, sticky=tk.W, padx=(30,0))

        self.frameDOWNButton = tk.Button(sweepFrame, text="<", bg='#bfc6db', fg='#323338',font=widgetButtonFont)
        self.frameDOWNButton.grid(row=16, column=2, pady=5, sticky= tk.W)

        self.frameUPButton = tk.Button(sweepFrame, text=">", bg='#bfc6db', fg='#323338', font=widgetButtonFont)
        self.frameUPButton.grid(row=16, column=2, padx=(30,0), pady=2, sticky= tk.W)


        self.frameLASTButton = tk.Button(sweepFrame, text=">>", bg='#bfc6db', fg='#323338', font=widgetButtonFont)
        self.frameLASTButton.grid(row=16, column=3,  pady=2, sticky=tk.W)


        self.runButton = tk.Button(sweepFrame, text="Run", bg='#bfc6db', fg='#323338', font=widgetButtonFont, width=15, command= self.startMeasure)
        self.runButton.grid(row=17, column=1, columnspan=3, pady=(30,20), sticky=tk.W)

        self.visibility()

    def check(self, textvalue):
        try:
            int(textvalue)
            return True
        except:
            return False

    def startMeasure(self):
        measure = False

        if (self.check(self.startEntry.get())):
            self.startEntry["bg"] = "white"
            measure = True
        else:
            self.startEntry["bg"] = "#cf5f5f"

        if (self.check(self.stopEntry.get())):
            self.stopEntry["bg"] = "white"
            measure = True
        else:
            self.stopEntry["bg"] = "#cf5f5f"

        if (self.check(self.pointsEntry.get())):
            self.pointsEntry["bg"] = "white"
            measure = True
        else:
            self.pointsEntry["bg"] = "#cf5f5f"

        if (measure):
            print("start measure")
            if (self.runButton["text"] == "Run"):
                self.runMeasure()
            else:
                if (self.continuous.get() == 1):
                    self.stopMeasure()
        else:
            print("bad input")

    def stopMeasure(self):
        print("zastavujem meranie")
        self.runButton["text"] = "Run"

    def runMeasure(self):
        print(" - mode:")
        if (self.continuous.get() == 1):
            print("continuous")
        else:
            print("not continous")

        if (self.autosave.get() == 1):
            print("autosave")
        else:
            print("not autosave")

        print("")
        if (self.freqVariable.get() == 0):
            print("in MHz")
        else:
            print("in GHz")
        print("start:", self.startEntry.get())
        print("stop:", self.stopEntry.get())
        print("points:", self.pointsEntry.get())
        print("")
        print(" - S parameters:")
        if (self.s11.get() == 1):
            print("S11")
        if (self.s12.get() == 1):
            print("S12")
        if (self.s21.get() == 1):
            print("S21")
        if (self.s22.get() == 1):
            print("S22")

        if (self.continuous.get()==1):
            self.runButton["text"] = "Stop"

        #self.saveSettings()

    def visibility(self):
        if (self.continuous.get() == 0):
            self.frameEntry.delete(0,tk.END)
            self.frameEntry.insert(tk.END,"0")
            self.frameEntry["state"] = tk.DISABLED
            self.maxLabel["text"] = "/ ---"
            self.frameDOWNButton["state"] = tk.DISABLED
            self.frameUPButton["state"] = tk.DISABLED
            self.frameLASTButton["state"] = tk.DISABLED
        else:
            self.frameEntry.delete(0, tk.END)
            self.frameEntry.insert(tk.END,"0")              #TO DO: aktualny frame
            self.frameEntry["state"] = tk.NORMAL
            self.maxLabel["text"] = "/ 10"                   #TO DO: počet Frame
            self.frameDOWNButton["state"] = tk.NORMAL
            self.frameUPButton["state"] = tk.NORMAL
            self.frameLASTButton["state"] = tk.NORMAL