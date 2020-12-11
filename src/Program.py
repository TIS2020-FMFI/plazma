class Program:
    def __init__(self):
        print("Start")
        self.adapter = Adapter()
        self.project = Project()
        self.gui = Gui(self)

    def metoda(self):
        # self.adapter.get_state()
        pass


if __name__ == '__main__':
    main = Program()
