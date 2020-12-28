from adapter import Adapter
from project import Project
from gui import Gui
from file_manager import FileManager
import threading
import queue
import time


class Program:
    def __init__(self):
        self.adapter = Adapter(self)
        self.project = Project(self)
        self.gui = Gui(self)
        self.file_manager = FileManager(self)

        self.function_queue = queue.Queue()
        self.work_thread = threading.Thread(target=self.execute_functions)
        self.work_thread.daemon = True
        self.work_thread.start()

    def execute_functions(self):
        while True:
            try:
                function = "self." + self.function_queue.get()
                exec(function)
            except queue.Empty:
                time.sleep(0.5)

    def queue_function(self, function):
        self.function_queue.put(function)

    def connect(self, address):
        if self.adapter.connect(address):
            self.gui.gpib.update_button_connected()
            self.gui.info.change_connect_label()
        else:
            print('Nepodarilo sa pripojit')

    def disconnect(self):
        self.adapter.disconnect()
        self.gui.gpib.update_button_disconnected()
        self.gui.info.change_connect_label()

    def save_state(self):
        # TODO ci sa podarilo alebo nie
        self.project.set_state(self.adapter.get_state())
        self.gui.info.change_state_label()

    def recall_state(self):
        if self.adapter.set_state(self.project.get_state()):
            # tak posli spatnu vazbu GUIcku
            pass
        else:
            print('Nepodarilo sa poslat stav')



if __name__ == '__main__':
    main = Program()
    main.gui.window.mainloop()
