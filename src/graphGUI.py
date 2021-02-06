import tkinter as tk
import tkinter.font as tk_font
from graphs import Graphs


class GraphsGui:
    def __init__(self, main_gui):
        self.main_gui = main_gui

        widget_label_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font_small)
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=self.main_gui.label_font, weight="bold")

        graph_frame = tk.LabelFrame(self.main_gui.window, text="GRAPHS",
                                    fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        graph_frame.grid(row=0, column=3, sticky=tk.N, rowspan=15,  padx=(self.main_gui.padx, 0))

        self.graph1 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        self.graph1.grid(row=0, column=0, sticky=tk.N, rowspan=4)

        self.graph1_plot = Graphs(self.graph1, self.main_gui.program, self.main_gui.fig_a, self.main_gui.fig_b)

        self.graph1_variable = tk.IntVar()
        self.graph1_XYY_radiobutton = tk.Radiobutton(self.graph1, text="XYY", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph1_plot_draw, variable=self.graph1_variable,
                                                     value=0, font=widget_label_font)
        self.graph1_Smith_radiobutton = tk.Radiobutton(self.graph1, text="Smith", bg='#f2f3fc', fg="#323338",
                                                       command=self.graph1_plot_draw, variable=self.graph1_variable,
                                                       value=1, font=widget_label_font)

        self.graph1_XYY_radiobutton.grid(row=3, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)
        self.graph1_Smith_radiobutton.grid(row=4, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)

        self.graph1_s_variable = tk.IntVar()
        self.graph1_S11_radiobutton = tk.Radiobutton(self.graph1, text="S11", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph1_plot_draw, variable=self.graph1_s_variable,
                                                     value=0, font=widget_label_font)
        self.graph1_S11_radiobutton.select()
        self.graph1_S12_radiobutton = tk.Radiobutton(self.graph1, text="S12", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph1_plot_draw, variable=self.graph1_s_variable,
                                                     value=1, font=widget_label_font)
        self.graph1_S21_radiobutton = tk.Radiobutton(self.graph1, text="S21", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph1_plot_draw, variable=self.graph1_s_variable,
                                                     value=2, font=widget_label_font)
        self.graph1_S22_radiobutton = tk.Radiobutton(self.graph1, text="S22", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph1_plot_draw, variable=self.graph1_s_variable,
                                                     value=3, font=widget_label_font)
        self.graph1_s_variable.set(0)

        self.graph1_S11_radiobutton.grid(row=3, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph1_S12_radiobutton.grid(row=3, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph1_S21_radiobutton.grid(row=4, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph1_S22_radiobutton.grid(row=4, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        self.graph2 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        self.graph2.grid(row=0, column=1, sticky=tk.N, rowspan=5)

        self.graph2_plot = Graphs(self.graph2, self.main_gui.program, self.main_gui.fig_a, self.main_gui.fig_b)

        self.graph2_variable = tk.IntVar()
        self.graph2_XYY_radiobutton = tk.Radiobutton(self.graph2, text="XYY", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph2_plot_draw, variable=self.graph2_variable,
                                                     value=0, font=widget_label_font)
        self.graph2_Smith_radiobutton = tk.Radiobutton(self.graph2, text="Smith", bg='#f2f3fc', fg="#323338",
                                                       command=self.graph2_plot_draw, variable=self.graph2_variable,
                                                       value=1, font=widget_label_font)

        self.graph2_XYY_radiobutton.grid(row=3, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)
        self.graph2_Smith_radiobutton.grid(row=4, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)

        self.graph2_s_variable = tk.IntVar()
        self.graph2_S11_radiobutton = tk.Radiobutton(self.graph2, text="S11", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph2_plot_draw, variable=self.graph2_s_variable,
                                                     value=0, font=widget_label_font)
        self.graph2_S12_radiobutton = tk.Radiobutton(self.graph2, text="S12", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph2_plot_draw, variable=self.graph2_s_variable,
                                                     value=1, font=widget_label_font)
        self.graph2_S12_radiobutton.select()
        self.graph2_S21_radiobutton = tk.Radiobutton(self.graph2, text="S21", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph2_plot_draw, variable=self.graph2_s_variable,
                                                     value=2, font=widget_label_font)
        self.graph2_S22_radiobutton = tk.Radiobutton(self.graph2, text="S22", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph2_plot_draw, variable=self.graph2_s_variable,
                                                     value=3, font=widget_label_font)

        self.graph2_S11_radiobutton.grid(row=3, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph2_S12_radiobutton.grid(row=3, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph2_S21_radiobutton.grid(row=4, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph2_S22_radiobutton.grid(row=4, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        self.graph3 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        self.graph3.grid(row=5, column=0, sticky=tk.N, rowspan=5)

        self.graph3_plot = Graphs(self.graph3, self.main_gui.program, self.main_gui.fig_a, self.main_gui.fig_b)

        self.graph3_variable = tk.IntVar()
        self.graph3_XYY_radiobutton = tk.Radiobutton(self.graph3, text="XYY", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph3_plot_draw, variable=self.graph3_variable,
                                                     value=0, font=widget_label_font)
        self.graph3_Smith_radiobutton = tk.Radiobutton(self.graph3, text="Smith", bg='#f2f3fc', fg="#323338",
                                                       command=self.graph3_plot_draw, variable=self.graph3_variable,
                                                       value=1, font=widget_label_font)

        self.graph3_XYY_radiobutton.grid(row=8, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)
        self.graph3_Smith_radiobutton.grid(row=9, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)

        self.graph3_s_variable = tk.IntVar()
        self.graph3_S11_radiobutton = tk.Radiobutton(self.graph3, text="S11", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph3_plot_draw, variable=self.graph3_s_variable,
                                                     value=0, font=widget_label_font)
        self.graph3_S12_radiobutton = tk.Radiobutton(self.graph3, text="S12", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph3_plot_draw, variable=self.graph3_s_variable,
                                                     value=1, font=widget_label_font)
        self.graph3_S21_radiobutton = tk.Radiobutton(self.graph3, text="S21", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph3_plot_draw, variable=self.graph3_s_variable,
                                                     value=2, font=widget_label_font)
        self.graph3_S21_radiobutton.select()
        self.graph3_S22_radiobutton = tk.Radiobutton(self.graph3, text="S22", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph3_plot_draw, variable=self.graph3_s_variable,
                                                     value=3, font=widget_label_font)

        self.graph3_S11_radiobutton.grid(row=8, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph3_S12_radiobutton.grid(row=8, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph3_S21_radiobutton.grid(row=9, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph3_S22_radiobutton.grid(row=9, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

        self.graph4 = tk.LabelFrame(graph_frame, fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        self.graph4.grid(row=6, column=1, sticky=tk.N, rowspan=5)

        self.graph4_plot = Graphs(self.graph4, self.main_gui.program, self.main_gui.fig_a, self.main_gui.fig_b)

        self.graph4_variable = tk.IntVar()
        self.graph4_XYY_radiobutton = tk.Radiobutton(self.graph4, text="XYY", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph4_plot_draw, variable=self.graph4_variable,
                                                     value=0, font=widget_label_font)
        self.graph4_Smith_radiobutton = tk.Radiobutton(self.graph4, text="Smith", bg='#f2f3fc', fg="#323338",
                                                       command=self.graph4_plot_draw, variable=self.graph4_variable,
                                                       value=1, font=widget_label_font)

        self.graph4_XYY_radiobutton.grid(row=8, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)
        self.graph4_Smith_radiobutton.grid(row=9, column=0, padx=(self.main_gui.fig_padx, 0), sticky=tk.N + tk.W)

        self.graph4_s_variable = tk.IntVar()
        self.graph4_S11_radiobutton = tk.Radiobutton(self.graph4, text="S11", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph4_plot_draw, variable=self.graph4_s_variable,
                                                     value=0, font=widget_label_font)
        self.graph4_S12_radiobutton = tk.Radiobutton(self.graph4, text="S12", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph4_plot_draw, variable=self.graph4_s_variable,
                                                     value=1, font=widget_label_font)
        self.graph4_S21_radiobutton = tk.Radiobutton(self.graph4, text="S21", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph4_plot_draw, variable=self.graph4_s_variable,
                                                     value=2, font=widget_label_font)
        self.graph4_S22_radiobutton = tk.Radiobutton(self.graph4, text="S22", bg='#f2f3fc', fg="#323338",
                                                     command=self.graph4_plot_draw, variable=self.graph4_s_variable,
                                                     value=3, font=widget_label_font)
        self.graph4_S22_radiobutton.select()

        self.graph4_S11_radiobutton.grid(row=8, column=0, padx=(10, 0), sticky=tk.N + tk.W)
        self.graph4_S12_radiobutton.grid(row=8, column=0, padx=(70, 0), sticky=tk.N + tk.W)
        self.graph4_S21_radiobutton.grid(row=9, column=0, padx=(10, 0), pady=(0, 0), sticky=tk.N + tk.W)
        self.graph4_S22_radiobutton.grid(row=9, column=0, padx=(70, 0), pady=(0, 0), sticky=tk.N + tk.W)

    def graph4_plot_draw(self):
        if self.graph4_variable.get() == 0:
            self.graph4_plot.set_type("XYY")
        else:
            self.graph4_plot.set_type("Smith")

        if self.graph4_s_variable.get() == 0:
            self.graph4_plot.set_s_param("S11")
        elif self.graph4_s_variable.get() == 1:
            self.graph4_plot.set_s_param("S12")
        elif self.graph4_s_variable.get() == 2:
            self.graph4_plot.set_s_param("S21")
        else:
            self.graph4_plot.set_s_param("S22")

        self.graph4_plot.draw_measurement(self.main_gui.sweep.current_frame - 1)

    def graph3_plot_draw(self):
        if self.graph3_variable.get() == 0:
            self.graph3_plot.set_type("XYY")
        else:
            self.graph3_plot.set_type("Smith")

        if self.graph3_s_variable.get() == 0:
            self.graph3_plot.set_s_param("S11")
        elif self.graph3_s_variable.get() == 1:
            self.graph3_plot.set_s_param("S12")
        elif self.graph3_s_variable.get() == 2:
            self.graph3_plot.set_s_param("S21")
        else:
            self.graph3_plot.set_s_param("S22")

        self.graph3_plot.draw_measurement(self.main_gui.sweep.current_frame - 1)

    def graph2_plot_draw(self):
        if self.graph2_variable.get() == 0:
            self.graph2_plot.set_type("XYY")
        else:
            self.graph2_plot.set_type("Smith")

        if self.graph2_s_variable.get() == 0:
            self.graph2_plot.set_s_param("S11")
        elif self.graph2_s_variable.get() == 1:
            self.graph2_plot.set_s_param("S12")
        elif self.graph2_s_variable.get() == 2:
            self.graph2_plot.set_s_param("S21")
        else:
            self.graph2_plot.set_s_param("S22")

        self.graph2_plot.draw_measurement(self.main_gui.sweep.current_frame - 1)

    def graph1_plot_draw(self):
        if self.graph1_variable.get() == 0:
            self.graph1_plot.set_type("XYY")
        else:
            self.graph1_plot.set_type("Smith")

        if self.graph1_s_variable.get() == 0:
            self.graph1_plot.set_s_param("S11")
        elif self.graph1_s_variable.get() == 1:
            self.graph1_plot.set_s_param("S12")
        elif self.graph1_s_variable.get() == 2:
            self.graph1_plot.set_s_param("S21")
        else:
            self.graph1_plot.set_s_param("S22")

        self.graph1_plot.draw_measurement(self.main_gui.sweep.current_frame-1)

    def refresh_all_graphs(self):
        self.graph1_plot_draw()
        self.graph2_plot_draw()
        self.graph3_plot_draw()
        self.graph4_plot_draw()
