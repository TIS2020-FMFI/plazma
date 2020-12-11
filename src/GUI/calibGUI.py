import tkinter as tk
from tkinter import filedialog
import tkinter.font as tkFont

class CalibrationGUI:

    def __init__(self, program):
        self.p = program
        self.createCalibGUI()

    def createCalibGUI(self):
        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetButtonFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetLabelBoldFont = tkFont.Font(family="Tw Cen MT", size=14, weight="bold")
        widgetPortFont = tkFont.Font(family="Tw Cen MT", size=11)

        calibrationFrame = tk.LabelFrame(self.p.window, text="CALIBRATION", fg="#323338", bg='#f2f3fc', font=widgetTitlefont, relief=tk.RIDGE)
        calibrationFrame.grid(row=0, rowspan=1, column=2, sticky=tk.N, padx=5)

        saveCalibButton = tk.Button(calibrationFrame, text="SAVE CALIBRATION", font=widgetButtonFont, bg='#bfc6db', fg='#323338', command=self.saveCalibration)
        saveCalibButton.grid(row=0, column=0, pady=5, padx=5)

        loadCalibButton = tk.Button(calibrationFrame, text="LOAD CALIBRATION", font=widgetButtonFont, bg='#bfc6db', fg='#323338', command=self.loadCalibration)
        loadCalibButton.grid(row=0, column=1, pady=5, padx=5)

        lineLabel = tk.Label(calibrationFrame, text="_______________________________________________________________", fg="#b3b3b5", bg="#f2f3fc")
        lineLabel.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        portExtensionLabel = tk.Label(calibrationFrame, text="Port Extension bar", fg='#323338', bg="#f2f3fc", font=widgetLabelBoldFont)
        portExtensionLabel.grid(row=2, column=0, columnspan=3, sticky=tk.W + tk.N, padx=5)

        port1LengthLabel = tk.Label(calibrationFrame, text="PORT1 Length/(m):", fg='#323338', bg="#f2f3fc", font=widgetPortFont)
        port1LengthLabel.grid(row=3, column=0, sticky=tk.W + tk.N, padx=5)

        self.port1 = tk.StringVar()
        self.port1spinBox = tk.Spinbox(calibrationFrame, textvariable = self.port1,from_=0.0000, to=1.0000, increment=0.0001, format="%.4f", justify=tk.RIGHT, font=widgetPortFont)
        self.port1spinBox.grid(row=3, column=1, sticky=tk.W, pady=3)

        port2LengthLabel = tk.Label(calibrationFrame, text="PORT2 Length/(m): ", fg='#323338', bg="#f2f3fc",
                                    font=widgetPortFont)
        port2LengthLabel.grid(row=4, column=0, sticky=tk.W + tk.N, padx=5)

        self.port2 = tk.StringVar()
        self.port2spinBox = tk.Spinbox(calibrationFrame, textvariable = self.port2, from_=0.0000, to=1.0000, increment=0.0001, format="%.4f", justify=tk.RIGHT, font=widgetPortFont)
        self.port2spinBox.grid(row=4, column=1, sticky=tk.W, pady=3)

        velocityFactorLabel = tk.Label(calibrationFrame, text="Velocity Factor:", fg='#323338', bg="#f2f3fc",font=widgetPortFont)
        velocityFactorLabel.grid(row=5, column=0, sticky=tk.W + tk.N, padx=5, pady=5)

        self.velFact = tk.StringVar()
        self.velFactspinBox = tk.Spinbox(calibrationFrame, textvariable= self.velFact, from_=0.00, to=1.00, increment=0.01, format="%.2f", justify=tk.RIGHT, font=widgetPortFont)
        self.velFactspinBox.grid(row=5, column=1, sticky=tk.W, pady=3)

        self.aCal = tk.IntVar()
        self.adjustCalCheckbutton = tk.Checkbutton(calibrationFrame, text="Adjust Calibration", variable=self.aCal, onvalue=1, offvalue=0, font=widgetPortFont, bg='#f2f3fc')
        self.adjustCalCheckbutton.grid(row=6, column=1, sticky=tk.W, pady=5)
        self.adjustCalCheckbutton["command"] = self.adjustCalibration


    def validate(self,spinboxValue):
        try:
            float(spinboxValue)
            return True
        except:
            return False


    def adjustCalibration(self):
        if (self.aCal.get() == 0):
            self.velFactspinBox["fg"] = "black"
            self.port1spinBox["fg"] = "black"
            self.port2spinBox["fg"] = "black"
        else:
            if (self.validate(self.velFactspinBox.get())):
                self.velFactspinBox["fg"] = "black"
            else:
                self.velFactspinBox["fg"] = "red"

            if (self.validate(self.port1spinBox.get())):
                self.port1spinBox["fg"] = "black"
            else:
                self.port1spinBox["fg"] = "red"

            if (self.validate(self.port2spinBox.get())):
                self.port2spinBox["fg"] = "black"
            else:
                self.port2spinBox["fg"] = "red"

    def saveCalibration(self):
        #TO DO: save calibration
        print("Ukladám kalilbráciu do pamäte")

    def loadCalibration(self):
        # TO DO: load calibration
        print("Načítavam kalibráciu")