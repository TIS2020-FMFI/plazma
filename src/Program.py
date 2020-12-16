from adapter import Adapter
from project import Project
from gui import Gui
from file_manager import FileManager


class Program:
    def __init__(self):
        print("Start")
        self.adapter = Adapter(self)
        self.project = Project(self)
        self.gui = Gui(self)
        self.file_manager = FileManager(self)


if __name__ == '__main__':
    main = Program()
    main.gui.window.mainloop()
