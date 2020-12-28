import os
from datetime import datetime


class FileManager:
    def __init__(self, program):
        self.settings = None    #  Dictionary, dict()
        self.program = program

    def save_project(self, path, name, description):
        # doplnit a osetrit vsetky vynimky
        # co ked taky path uz existuje...? pridat ku menu cislo...?
        pass

    def load_project(self, path):
        pass

    def get_settings(self):
        return self.settings

    def set_settings(self, address, port1, port2, vel_factor, freq_unit, freq_start, freq_stop,
                     points, parameter_format, parameters, continuous, autosave):
        self.settings["address"] = address
        ...
        pass

    def save_settings(self):
        # vyrobi settings.txt v kazdom riadku  kluc = hodnota
        # address = 16
        pass


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