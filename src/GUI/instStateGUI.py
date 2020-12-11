import tkinter as tk
import tkinter.font as tkFont

class InstState:
    def __init__(self, program):
        self.p = program
        self.createInstStateGUI()


    def createInstStateGUI(self):
        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetLabelFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetButtonFont = tkFont.Font(family="Tw Cen MT", size=13)

        InstStateFrame = tk.LabelFrame(self.p.window, text="INST. STATE", fg="#323338", bg='#f2f3fc', font=widgetTitlefont, relief=tk.RIDGE)
        InstStateFrame.grid(row=0, column=1, pady=(150, 0))

        self.presetButton = tk.Button(InstStateFrame, text="PRESET", bg='#bfc6db', fg='#323338', font=widgetButtonFont, command=self.preset)
        self.presetButton.grid(row=0, column=1, padx=10, pady=10)

        lineLabel = tk.Label(InstStateFrame, text="_____________________________________________________", fg="#b3b3b5", bg="#f2f3fc")
        lineLabel.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        connectLabel = tk.Label(InstStateFrame, text="Connect:", fg="#323338", bg="#f2f3fc", font=widgetLabelFont)
        connectLabel.grid(row=2, column=0, sticky=tk.W + tk.N, padx=10, pady=10)

        self.connectButton = tk.Button(InstStateFrame, text="ON", bg="#5bb38a", fg='#323338', font=widgetButtonFont)
        self.connectButton["command"] = self.connect
        self.connectButton.grid(row=2, column=2, padx=10)


        lineLabel = tk.Label(InstStateFrame, text="_____________________________________________________", fg="#b3b3b5", bg="#f2f3fc")
        lineLabel.grid(row=3, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        self.saveStateButton = tk.Button(InstStateFrame, text="SAVE STATE", bg='#bfc6db', fg='#323338', font=widgetButtonFont, command=self.saveState)
        self.saveStateButton.grid(row=4, column=1, columnspan=1, pady=2)

        self.recallStateButton = tk.Button(InstStateFrame, text="RECALL STATE", bg='#bfc6db', fg='#323338', font=widgetButtonFont, command=self.recallState)
        self.recallStateButton.grid(row=5, column=1, columnspan=1, pady=5)

    def disable(self):
        self.presetButton["state"] = tk.DISABLED
        self.saveStateButton["state"] = tk.DISABLED
        self.recallStateButton["state"] = tk.DISABLED

    def enable(self):
        self.presetButton["state"] = tk.DISABLED
        self.saveStateButton["state"] = tk.DISABLED
        self.recallStateButton["state"] = tk.DISABLED

    def preset(self):

        #TO DO: Preset prístroja
        print("Vykonávam preset prístroja")

    def connect(self):

        # TO DO: connect/disconnect prístroja
        if (self.p.connect):
            print("Odpájam prístroj")
            self.connectButton["text"] = "ON"
            self.connectButton["bg"] = "#5bb38a"


        else:
            print("Pripájam prístroj")
            self.connectButton['text'] = "OFF"
            self.connectButton["bg"] = "#b5555a"



        self.p.connect = not self.p.connect
        print(self.p.connect)


    def saveState(self):

        # TO DO: Uložiť stav prístroja prístroja
        print("ukladám stav prístroja do pamäte")

    def recallState(self):

        # TO DO: Načítať uložený stav do prístroja
        print("načítavam stav z pamäte do prístroja")