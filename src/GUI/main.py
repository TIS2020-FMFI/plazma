import tkinter as tk
from calibGUI import CalibrationGUI
from gpibGUI import GPIB
from instStateGUI import InstState
from graphGUI import graphsGUI
from projectGUI import projectGUI
from sweepGUI import SweepGUI


class GUI:
    def __init__(self):
        self.window = tk.Tk()
        self.window.title("HP - 8753B")
        self.window.config(bg="white")
        self.connect = False


        self.createWidgets()

    def createWidgets(self):

        self.window['padx'] = 5
        self.window['pady'] = 5

        # GPIB
        self.GPIB = GPIB(self)

        # INSTRUMENT STATE
        self.STATE = InstState(self)

        # PROJECT
        self.PROJECT = projectGUI(self)

        # CALIBRATIONPART
        self.CALIBRATION = CalibrationGUI(self)

        # SWEEP
        self.SWEEP = SweepGUI(self)

        # GRAPHS
        self.GRAPHS = graphsGUI(self)

        """if (self.connect is False):
            self.disableButtons()
        else:
            self.enableButtons()"""

    def disableButtons(self):
        self.GPIB.disable()
        self.STATE.disable()


    def enableButtons(self):
        self.GPIB.enable()
        self.STATE.enable()

    def connectionControl(self):
        print("ahoj")
        #TO DO:
        return 0


if __name__ == '__main__':
    HP = GUI()
    HP.window.mainloop()

