import os
from datetime import datetime
from tkinter import filedialog


class FileManager:
    def __init__(self, program):
        # self.settings = {}  # None    #  Dictionary, dict()
        self.filepath = None
        self.program = program

    def save_project(self, path, name, description):
        self.filepath = os.path.join(path, name)
        now = datetime.now()
        date = datetime.date(now)
        if not os.path.exists(self.filepath):
            os.mkdir(self.filepath)
            name = "description.txt"
            file_path = os.path.join(self.filepath, name)
            f = open(file_path, "w")
            f.write(description)
            f.close()
        else:
            os.mkdir(self.filepath + " " + str(date))
            name = "description.txt"
            file_path = os.path.join(self.filepath + " " + str(date), name)
            f = open(file_path, "w")
            f.write(description)
            f.close()
            self.filepath = self.filepath + " " + str(date)



        # doplnit a osetrit vsetky vynimky
        # co ked taky path uz existuje...? pridat ku menu cislo...?
        self.save_settings()
        pass

    def load_project(self): #path

        directory = filedialog.askdirectory()
        self.filepath = directory
        print(self.filepath)
        pass

    # def get_settings(self):
    #     return self.settings
    #
    # def set_settings(self, address, port1, port2, vel_factor, freq_unit, freq_start, freq_stop,
    #                  points, parameter_format, parameters, continuous, autosave):
    #     self.settings["address"] = address
    #     self.settings["port1"] = port1
    #     self.settings["port2"] = port2
    #     self.settings["vel_factor"] = vel_factor
    #     self.settings["freq_unit"] = freq_unit
    #     self.settings["freq_start"] = freq_start
    #     self.settings["freq_stop"] = freq_stop
    #     self.settings["points"] = points
    #     self.settings["parameter_format"] = parameter_format
    #     self.settings["parameters"] = parameters
    #     self.settings["continuous"] = continuous
    #     self.settings["autosave"] = autosave
    #     ...
    #     pass

    # def save_settings(self):
    #     if self.filepath is not None:
    #         print(self.filepath)
    #         file = open(self.filepath + "\\" + "settings.txt", "w")
    #         for key, value in self.settings.items():
    #             file.write(str(key) + " = " + str(value) + "\n")
    #         file.close()
    #     else:
    #         file = open("settings.txt", "w")
    #         for key, value in self.settings.items():
    #             file.write(str(key) + " = " + str(value) + "\n")
    #         file.close()
    #     # vyrobi settings.txt v kazdom riadku  kluc = hodnota
    #     # address = 16
    #     pass


#s = FileManager()
#s.save_project("C:\\Users\\jozef\\Desktop","Projekkkt","descriptiiiiiiii")
#s.save_project("D:\\Fakulta\\3_rocnik\\TIS\\zzzzProjekt\\plazma - Copy\\src","Projekkkt","descriptiiiiiiii")
#print(datetime.now())
#s.load_project()
#s.set_settings(10,20,30,40,50,50,5,0,4,986,51,"ese")
#s.save_settings()
# def save(self):
#     # TODO: save project
#
#     folder_name = tk.filedialog.askdirectory()
#     if self.project_name_entry.get() == "Project Name" or self.project_name_entry.get() == "":
#         now = datetime.now()
#         date = datetime.date(now)
#
#         file_name = "PROJECT - " + str(date)
#         path = os.path.join(folder_name, file_name)
#         try:
#             os.mkdir(path)
#         except FileExistsError:
#             os.mkdir(path)
#     else:
#         file_name = self.project_name_entry.get()
#         path = os.path.join(folder_name, file_name)
#         os.mkdir(path)
#
#     self.save_description(path)
#
# def save_description(self, path):
#
#     if (self.project_descrip_text.get(1.0, tk.END) != "Project Name" and self.project_descrip_text.get(1.0,
#                                                                                                        tk.END) != ""):
#         name = "description.txt"
#         file_path = os.path.join(path, name)
#         f = open(file_path, "w")
#         f.write(self.project_descrip_text.get(1.0, tk.END))
#         f.close()
#
# def load(self):
#     # TODO: load project
#
#     folderName = tk.filedialog.askdirectory()
