import tkinter as tk
import tkinter.font as tk_font

from PIL import ImageTk, Image


class GraphsGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui
        self.create_graphs_gui()

    def create_graphs_gui(self):
        widget_label_font = tk_font.Font(family="Tw Cen MT", size=11)
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=13, weight="bold")
        widget_scale_font = tk_font.Font(family="Tw Cen MT", size=11, weight="bold")

        graph_frame = tk.LabelFrame(self.main_gui.window, text="GRAPHS",
                                    fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        graph_frame.grid(row=0, column=3, sticky=tk.N, rowspan=15)

        image = Image.open("graf.png")
        photo = ImageTk.PhotoImage(image)

        # TODO: GRAPH n.1:

        self.graph1 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE,
                                    width=400, height=300)
        self.graph1.grid(row=0, column=0, sticky=tk.N, rowspan=5)

        # TODO: Vložiť graf
        self.graph1_plot = tk.Label(self.graph1, image=photo, width=400, height=300)
        self.graph1_plot.image = photo
        self.graph1_plot.grid(row=0, column=0, padx=10, pady=10)

        self.graph1_variable = tk.IntVar()
        self.graph1_XYY_radiobutton = tk.Radiobutton(self.graph1, text="XYY", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                     variable=self.graph1_variable, value=0, font=widget_label_font)
        self.graph1_Smith_radiobutton = tk.Radiobutton(self.graph1, text="Smith", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                       variable=self.graph1_variable, value=1, font=widget_label_font)

        self.graph1_XYY_radiobutton.grid(row=1, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph1_Smith_radiobutton.grid(row=1, column=0, padx=(70, 0), sticky=tk.N + tk.W)

        line_label = tk.Label(self.graph1, text="|", fg="#b3b3b5", bg="#f2f3fc", font=widget_title_font)
        line_label.grid(row=1, column=0, sticky=tk.N + tk.W, padx=(140, 0))

        self.graph1_autoscale = tk.IntVar()
        self.graph1_autoscale_checkbox = tk.Checkbutton(self.graph1, variable=self.graph1_autoscale, text="autoscale",
                                                        font=widget_label_font, bg='#f2f3fc',
                                                        command=self.change_graph1_scale_status)
        self.graph1_autoscale_checkbox.grid(row=1, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(170, 0))

        self.graph1_s_variable = tk.IntVar()
        self.graph1_S11_radiobutton = tk.Radiobutton(self.graph1, text="S11", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                     variable=self.graph1_s_variable, value=0, font=widget_label_font)
        self.graph1_S12_radiobutton = tk.Radiobutton(self.graph1, text="S12", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                     variable=self.graph1_s_variable, value=1, font=widget_label_font)
        self.graph1_S21_radiobutton = tk.Radiobutton(self.graph1, text="S21", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                     variable=self.graph1_s_variable, value=2, font=widget_label_font)
        self.graph1_S22_radiobutton = tk.Radiobutton(self.graph1, text="S22", bg='#f2f3fc', fg="#323338", command=self.graph1_plot_draw,
                                                     variable=self.graph1_s_variable, value=3, font=widget_label_font)
        self.graph1_s_variable.set(0)

        self.graph1_S11_radiobutton.grid(row=3, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph1_S12_radiobutton.grid(row=3, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph1_S21_radiobutton.grid(row=4, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph1_S22_radiobutton.grid(row=4, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        graph1_y1_label = tk.Label(self.graph1, text="Y1", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph1_y1_label.grid(row=2, column=0, sticky=tk.N + tk.W, padx=(275, 0))

        graph1_y2_label = tk.Label(self.graph1, text="Y2", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph1_y2_label.grid(row=2, column=0, sticky=tk.N + tk.W, padx=(325, 0))

        graph1_min_label = tk.Label(self.graph1, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph1_min_label.grid(row=3, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        graph1_max_label = tk.Label(self.graph1, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph1_max_label.grid(row=4, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        self.graph1_min_y1_entry = tk.Entry(self.graph1, width=5)
        self.graph1_min_y1_entry.grid(row=3, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph1_min_y1_entry["font"] = widget_label_font

        self.graph1_min_y2_entry = tk.Entry(self.graph1, width=5)
        self.graph1_min_y2_entry.grid(row=3, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph1_min_y2_entry["font"] = widget_label_font

        self.graph1_max_y1_entry = tk.Entry(self.graph1, width=5)
        self.graph1_max_y1_entry.grid(row=4, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph1_max_y1_entry["font"] = widget_label_font

        self.graph1_max_y2_entry = tk.Entry(self.graph1, width=5)
        self.graph1_max_y2_entry.grid(row=4, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph1_max_y2_entry["font"] = widget_label_font

        # TODO: GRAPH n.2:
        self.graph2 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE,
                                    width=400, height=300)
        self.graph2.grid(row=0, column=1, sticky=tk.N, rowspan=15)

        # TODO: vložiť graf2
        self.graph2_plot = tk.Label(self.graph2, image=photo, width=400, height=300)
        self.graph2_plot.image = photo
        self.graph2_plot.grid(row=0, column=0, padx=10, pady=10)

        self.graph2_variable = tk.IntVar()
        self.graph2_XYY_radiobutton = tk.Radiobutton(self.graph2, text="XYY", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                     variable=self.graph2_variable, value=0, font=widget_label_font)
        self.graph2_Smith_radiobutton = tk.Radiobutton(self.graph2, text="Smith", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                       variable=self.graph2_variable, value=1, font=widget_label_font)

        self.graph2_XYY_radiobutton.grid(row=1, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph2_Smith_radiobutton.grid(row=1, column=0, padx=(70, 0), sticky=tk.N + tk.W)

        line_label = tk.Label(self.graph2, text="|", fg="#b3b3b5", bg="#f2f3fc", font=widget_title_font)
        line_label.grid(row=1, column=0, sticky=tk.N + tk.W, padx=(140, 0))

        self.graph2_autoscale = tk.IntVar()
        self.graph2_autoscale_checkbox = tk.Checkbutton(self.graph2, variable=self.graph2_autoscale, text="autoscale",
                                                        font=widget_label_font, bg='#f2f3fc', command=self.change_graph2_scale_status)
        self.graph2_autoscale_checkbox.grid(row=1, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(170, 0))

        self.graph2_s_variable = tk.IntVar()
        self.graph2_S11_radiobutton = tk.Radiobutton(self.graph2, text="S11", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                     variable=self.graph2_s_variable, value=0, font=widget_label_font)
        self.graph2_S12_radiobutton = tk.Radiobutton(self.graph2, text="S12", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                     variable=self.graph2_s_variable, value=1, font=widget_label_font)
        self.graph2_S21_radiobutton = tk.Radiobutton(self.graph2, text="S21", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                     variable=self.graph2_s_variable, value=2, font=widget_label_font)
        self.graph2_S22_radiobutton = tk.Radiobutton(self.graph2, text="S22", bg='#f2f3fc', fg="#323338", command=self.graph2_plot_draw,
                                                     variable=self.graph2_s_variable, value=3, font=widget_label_font)


        self.graph2_S11_radiobutton.grid(row=3, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph2_S12_radiobutton.grid(row=3, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph2_S21_radiobutton.grid(row=4, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph2_S22_radiobutton.grid(row=4, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        graph2_y1_label = tk.Label(self.graph2, text="Y1", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph2_y1_label.grid(row=2, column=0, sticky=tk.N + tk.W, padx=(275, 0))

        graph2_y2_label = tk.Label(self.graph2, text="Y2", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph2_y2_label.grid(row=2, column=0, sticky=tk.N + tk.W, padx=(325, 0))

        graph2_min_label = tk.Label(self.graph2, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph2_min_label.grid(row=3, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        graph2_max_label = tk.Label(self.graph2, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph2_max_label.grid(row=4, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        self.graph2_min_y1_entry = tk.Entry(self.graph2, width=5)
        self.graph2_min_y1_entry.grid(row=3, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph2_min_y1_entry["font"] = widget_label_font

        self.graph2_min_y2_entry = tk.Entry(self.graph2, width=5)
        self.graph2_min_y2_entry.grid(row=3, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph2_min_y2_entry["font"] = widget_label_font

        self.graph2_max_y1_entry = tk.Entry(self.graph2, width=5)
        self.graph2_max_y1_entry.grid(row=4, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph2_max_y1_entry["font"] = widget_label_font

        self.graph2_max_y2_entry = tk.Entry(self.graph2, width=5)
        self.graph2_max_y2_entry.grid(row=4, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph2_max_y2_entry["font"] = widget_label_font

        # TODO: GRAPH n.3:
        self.graph3 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE,
                                    width=400, height=300)
        self.graph3.grid(row=5, column=0, sticky=tk.N, rowspan=5)

        # TODO: vložiť graf3
        self.graph3_plot = tk.Label(self.graph3, image=photo, width=400, height=300)
        self.graph3_plot.image = photo
        self.graph3_plot.grid(row=0, column=0, padx=10, pady=10)

        self.graph3_variable = tk.IntVar()
        self.graph3_XYY_radiobutton = tk.Radiobutton(self.graph3, text="XYY", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                     variable=self.graph3_variable, value=0, font=widget_label_font)
        self.graph3_Smith_radiobutton = tk.Radiobutton(self.graph3, text="Smith", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                       variable=self.graph3_variable, value=1, font=widget_label_font)

        self.graph3_XYY_radiobutton.grid(row=6, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph3_Smith_radiobutton.grid(row=6, column=0, padx=(70, 0), sticky=tk.N + tk.W)

        line_label = tk.Label(self.graph3, text="|", fg="#b3b3b5", bg="#f2f3fc", font=widget_title_font)
        line_label.grid(row=6, column=0, sticky=tk.N + tk.W, padx=(140, 0))

        self.graph3_autoscale = tk.IntVar()
        self.graph3_autoscale_checkbox = tk.Checkbutton(self.graph3, variable=self.graph3_autoscale, text="autoscale",
                                                        font=widget_label_font, bg='#f2f3fc', command=self.change_graph3_scale_status)
        self.graph3_autoscale_checkbox.grid(row=6, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(170, 0))

        self.graph3_s_variable = tk.IntVar()
        self.graph3_S11_radiobutton = tk.Radiobutton(self.graph3, text="S11", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                     variable=self.graph3_s_variable, value=0, font=widget_label_font)
        self.graph3_S12_radiobutton = tk.Radiobutton(self.graph3, text="S12", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                     variable=self.graph3_s_variable, value=1, font=widget_label_font)
        self.graph3_S21_radiobutton = tk.Radiobutton(self.graph3, text="S21", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                     variable=self.graph3_s_variable, value=2, font=widget_label_font)
        self.graph3_S22_radiobutton = tk.Radiobutton(self.graph3, text="S22", bg='#f2f3fc', fg="#323338", command=self.graph3_plot_draw,
                                                     variable=self.graph3_s_variable, value=3, font=widget_label_font)

        self.graph3_S11_radiobutton.grid(row=8, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph3_S12_radiobutton.grid(row=8, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph3_S21_radiobutton.grid(row=9, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph3_S22_radiobutton.grid(row=9, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        graph3_y1_label = tk.Label(self.graph3, text="Y1", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph3_y1_label.grid(row=7, column=0, sticky=tk.N + tk.W, padx=(275, 0))

        graph3_y2_label = tk.Label(self.graph3, text="Y2", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph3_y2_label.grid(row=7, column=0, sticky=tk.N + tk.W, padx=(325, 0))

        graph3_min_label = tk.Label(self.graph3, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph3_min_label.grid(row=8, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        graph3_max_label = tk.Label(self.graph3, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph3_max_label.grid(row=9, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        self.graph3_min_y1_entry = tk.Entry(self.graph3, width=5)
        self.graph3_min_y1_entry.grid(row=8, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph3_min_y1_entry["font"] = widget_label_font

        self.graph3_min_y2_entry = tk.Entry(self.graph3, width=5)
        self.graph3_min_y2_entry.grid(row=8, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph3_min_y2_entry["font"] = widget_label_font

        self.graph3_max_y1_entry = tk.Entry(self.graph3, width=5)
        self.graph3_max_y1_entry.grid(row=9, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph3_max_y1_entry["font"] = widget_label_font

        self.graph3_max_y2_entry = tk.Entry(self.graph3, width=5)
        self.graph3_max_y2_entry.grid(row=9, column=0, sticky=tk.N + tk.W, pady=(0, 10), padx=(320, 0))
        self.graph3_max_y2_entry["font"] = widget_label_font

        # TODO: GRAPH n.4:
        self.graph4 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE,
                                    width=400, height=300)
        self.graph4.grid(row=5, column=1, sticky=tk.N, rowspan=5)

        # TODO vložiť graf č.4:
        self.graph4_plot = tk.Label(self.graph4, image=photo, width=400, height=300)
        self.graph4_plot.image = photo
        self.graph4_plot.grid(row=0, column=0, padx=10, pady=10)


        self.graph4_variable = tk.IntVar()
        self.graph4_XYY_radiobutton = tk.Radiobutton(self.graph4, text="XYY", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                     variable=self.graph4_variable, value=0, font=widget_label_font)
        self.graph4_Smith_radiobutton = tk.Radiobutton(self.graph4, text="Smith", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                       variable=self.graph4_variable, value=1, font=widget_label_font)

        self.graph4_XYY_radiobutton.grid(row=6, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph4_Smith_radiobutton.grid(row=6, column=0, padx=(70, 0), sticky=tk.N + tk.W)

        line_label = tk.Label(self.graph4, text="|", fg="#b3b3b5", bg="#f2f3fc", font=widget_title_font)
        line_label.grid(row=6, column=0, sticky=tk.N + tk.W, padx=(140, 0))

        self.graph4_autoscale = tk.IntVar()
        self.graph4_autoscale_checkbox = tk.Checkbutton(self.graph4, variable=self.graph4_autoscale, text="autoscale",
                                                        font=widget_label_font, bg='#f2f3fc', command=self.change_graph4_scale_status)
        self.graph4_autoscale_checkbox.grid(row=6, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(170, 0))

        self.graph4_s_variable = tk.IntVar()
        self.graph4_S11_radiobutton = tk.Radiobutton(self.graph4, text="S11", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                     variable=self.graph4_s_variable, value=0, font=widget_label_font)
        self.graph4_S12_radiobutton = tk.Radiobutton(self.graph4, text="S12", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                     variable=self.graph4_s_variable, value=1, font=widget_label_font)
        self.graph4_S21_radiobutton = tk.Radiobutton(self.graph4, text="S21", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                     variable=self.graph4_s_variable, value=2, font=widget_label_font)
        self.graph4_S22_radiobutton = tk.Radiobutton(self.graph4, text="S22", bg='#f2f3fc', fg="#323338", command=self.graph4_plot_draw,
                                                     variable=self.graph4_s_variable, value=3, font=widget_label_font)

        self.graph4_S11_radiobutton.grid(row=8, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph4_S12_radiobutton.grid(row=8, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph4_S21_radiobutton.grid(row=9, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph4_S22_radiobutton.grid(row=9, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        graph4_y1_label = tk.Label(self.graph4, text="Y1", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph4_y1_label.grid(row=7, column=0, sticky=tk.N + tk.W, padx=(275, 0))

        graph4_y2_label = tk.Label(self.graph4, text="Y2", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph4_y2_label.grid(row=7, column=0, sticky=tk.N + tk.W, padx=(325, 0))

        graph4_min_label = tk.Label(self.graph4, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph4_min_label.grid(row=8, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        graph4_max_label = tk.Label(self.graph4, text="min", fg="black", bg="#f2f3fc", font=widget_scale_font)
        graph4_max_label.grid(row=9, column=0, sticky=tk.N + tk.W, padx=(230, 0))

        self.graph4_min_y1_entry = tk.Entry(self.graph4, width=5)
        self.graph4_min_y1_entry.grid(row=8, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph4_min_y1_entry["font"] = widget_label_font

        self.graph4_min_y2_entry = tk.Entry(self.graph4, width=5)
        self.graph4_min_y2_entry.grid(row=8, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(320, 0))
        self.graph4_min_y2_entry["font"] = widget_label_font

        self.graph4_max_y1_entry = tk.Entry(self.graph4, width=5)
        self.graph4_max_y1_entry.grid(row=9, column=0, sticky=tk.N + tk.W, pady=(0, 0), padx=(265, 0))
        self.graph4_max_y1_entry["font"] = widget_label_font

        self.graph4_max_y2_entry = tk.Entry(self.graph4, width=5)
        self.graph4_max_y2_entry.grid(row=9, column=0, sticky=tk.N + tk.W, pady=(0, 10), padx=(320, 0))
        self.graph4_max_y2_entry["font"] = widget_label_font

    def change_graph1_scale_status(self):
        if self.graph1_autoscale.get() == 1:
            self.graph1_max_y1_entry["state"] = tk.DISABLED
            self.graph1_max_y2_entry["state"] = tk.DISABLED
            self.graph1_min_y1_entry["state"] = tk.DISABLED
            self.graph1_min_y2_entry["state"] = tk.DISABLED
        else:
            self.graph1_max_y1_entry["state"] = tk.NORMAL
            self.graph1_max_y2_entry["state"] = tk.NORMAL
            self.graph1_min_y1_entry["state"] = tk.NORMAL
            self.graph1_min_y2_entry["state"] = tk.NORMAL

    def change_graph2_scale_status(self):
        if self.graph2_autoscale.get() == 1:
            self.graph2_max_y1_entry["state"] = tk.DISABLED
            self.graph2_max_y2_entry["state"] = tk.DISABLED
            self.graph2_min_y1_entry["state"] = tk.DISABLED
            self.graph2_min_y2_entry["state"] = tk.DISABLED
        else:
            self.graph2_max_y1_entry["state"] = tk.NORMAL
            self.graph2_max_y2_entry["state"] = tk.NORMAL
            self.graph2_min_y1_entry["state"] = tk.NORMAL
            self.graph2_min_y2_entry["state"] = tk.NORMAL

    def change_graph3_scale_status(self):
        if self.graph3_autoscale.get() == 1:
            self.graph3_max_y1_entry["state"] = tk.DISABLED
            self.graph3_max_y2_entry["state"] = tk.DISABLED
            self.graph3_min_y1_entry["state"] = tk.DISABLED
            self.graph3_min_y2_entry["state"] = tk.DISABLED
        else:
            self.graph3_max_y1_entry["state"] = tk.NORMAL
            self.graph3_max_y2_entry["state"] = tk.NORMAL
            self.graph3_min_y1_entry["state"] = tk.NORMAL
            self.graph3_min_y2_entry["state"] = tk.NORMAL

    def change_graph4_scale_status(self):
        if self.graph4_autoscale.get() == 1:
            self.graph4_max_y1_entry["state"] = tk.DISABLED
            self.graph4_max_y2_entry["state"] = tk.DISABLED
            self.graph4_min_y1_entry["state"] = tk.DISABLED
            self.graph4_min_y2_entry["state"] = tk.DISABLED
        else:
            self.graph4_max_y1_entry["state"] = tk.NORMAL
            self.graph4_max_y2_entry["state"] = tk.NORMAL
            self.graph4_min_y1_entry["state"] = tk.NORMAL
            self.graph4_min_y2_entry["state"] = tk.NORMAL

    def graph4_plot_draw(self):
        # TODO: kresliť graf č.4
        print("kreslim graf č.4")
        if self.graph4_variable.get() == 0:
            print("kreslim XYY")
        else:
            print("kreslim smith")

        if self.graph4_s_variable.get() == 0:
            print("kreslim S11")
        elif self.graph4_s_variable.get() == 1:
            print("kreslim S12")
        elif self.graph4_s_variable.get() == 2:
            print("kreslim S21")
        else:
            print("kreslim S22")

    def graph3_plot_draw(self):
        # TODO: kresliť graf č.3
        print("kreslim graf č.3")
        if self.graph3_variable.get() == 0:
            print("kreslim XYY")
        else:
            print("kreslim smith")

        if self.graph3_s_variable.get() == 0:
            print("kreslim S11")
        elif self.graph3_s_variable.get() == 1:
            print("kreslim S12")
        elif self.graph3_s_variable.get() == 2:
            print("kreslim S21")
        else:
            print("kreslim S22")

    def graph2_plot_draw(self):
        # TODO: kresliť graf č.2
        print("kreslim graf č.2")
        if self.graph2_variable.get() == 0:
            print("kreslim XYY")
        else:
            print("kreslim smith")

        if self.graph2_s_variable.get() == 0:
            print("kreslim S11")
        elif self.graph2_s_variable.get() == 1:
            print("kreslim S12")
        elif self.graph2_s_variable.get() == 2:
            print("kreslim S21")
        else:
            print("kreslim S22")

    def graph1_plot_draw(self):
        # TODO: kresliť graf č.1
        print("kreslim graf č.1")
        if self.graph1_variable.get() == 0:
            print("kreslim XYY")
        else:
            print("kreslim smith")

        if self.graph1_s_variable.get() == 0:
            print("kreslim S11")
        elif self.graph1_s_variable.get() == 1:
            print("kreslim S12")
        elif self.graph1_s_variable.get() == 2:
            print("kreslim S21")
        else:
            print("kreslim S22")