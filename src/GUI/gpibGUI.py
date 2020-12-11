import tkinter as tk
import tkinter.font as tkFont


class GPIB():
    def __init__(self, program):
        self.p = program
        self.createGpibGUI()

    def createGpibGUI(self):

        widgetTitlefont = tkFont.Font(family="Tw Cen MT", size=16, weight="bold")
        widgetLabelFont = tkFont.Font(family="Tw Cen MT", size=13)
        widgetButtonFont = tkFont.Font(family="Tw Cen MT", size=13)

        GPIB_frame = tk.LabelFrame(self.p.window, text="GPIB", fg="#323338", bg='#f2f3fc', font=widgetTitlefont, relief=tk.RIDGE )
        GPIB_frame.grid(row=0, column=1, sticky=tk.N)

        addressLabel = tk.Label(GPIB_frame, text="Address:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        addressLabel.grid(row=0, column=1, sticky=tk.W, padx=20)

        self.addressEntry = tk.Entry(GPIB_frame, width=10)
        self.addressEntry.insert(tk.END, "16")
        self.addressEntry["fg"] = "#2f3136"
        self.addressEntry["font"] = widgetLabelFont
        self.addressEntry.grid(row=0, column=1, sticky=tk.W, padx=(100,0), pady=3)

        self.addressButton = tk.Button(GPIB_frame, text="SET", bg='#bfc6db', fg='#323338', command=self.connectAddress)
        self.addressButton['font'] = widgetButtonFont
        self.addressButton.grid(row=0, column=2)

        lineLabel = tk.Label(GPIB_frame, text="_____________________________________________________", fg="#b3b3b5", bg="#f2f3fc")
        lineLabel.grid(row=1, column=0, columnspan=3, sticky=tk.W + tk.N, padx=10)

        GPIB_label = tk.Label(GPIB_frame, text="GPIB:", fg="#323338", bg='#f2f3fc', font=widgetLabelFont)
        GPIB_label.grid(row=2, column=1, sticky=tk.W, pady=10, padx=20)

        self.GPIB_button = tk.Button(GPIB_frame, text="OPEN", bg='#bfc6db', fg='#323338', command= self.openGPIBterminal)
        self.GPIB_button['font'] = widgetButtonFont
        self.GPIB_button.grid(row=2, column=2, padx=(10,20), pady=15)

    def disable(self):
        self.addressEntry["state"] = tk.DISABLED
        self.addressButton["state"] = tk.DISABLED
        self.GPIB_button["state"] = tk.DISABLED

    def enable(self):
        self.addressEntry["state"] = tk.NORMAL
        self.addressButton["state"] = tk.NORMAL
        self.GPIB_button["state"] = tk.NORMAL

    def openGPIBterminal(self):
        #TO DO: otvorít GPIB terminál

        print("Otváram GPIB terminál.")

    def connectAddress(self):

        if (self.validate(self.addressEntry.get())):
            #TO DO: pripojiť na adresu

            print("Pripájam sa na adresu: ", self.addressEntry.get())
        else:
            print("oprav adresu, zly format")

    def validate(self, addressInput):

        try:
            int(addressInput)
            self.addressEntry["bg"] = "white"
            return True
        except:
            self.addressEntry["bg"] = "#d44242"
            return False

