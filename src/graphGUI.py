import tkinter as tk
import tkinter.font as tk_font


class GraphsGui:
    def __init__(self, program):
        self.p = program
        self.create_graphs_gui()

    def create_graphs_gui(self):

        widget_label_font = tk_font.Font(family="Tw Cen MT", size=13)
        widget_title_font = tk_font.Font(family="Tw Cen MT", size=16, weight="bold")

        graph_frame = tk.LabelFrame(self.p.window, width=900, height=891, text="GRAPHS",
                                    fg="#323338", bg='#f2f3fc', font=widget_title_font, relief=tk.RIDGE)
        graph_frame.grid(row=0, column=3, rowspan=15)

        label = tk.Label(graph_frame, text="Tu budú grafy", fg='#323338', bg="#f2f3fc")
        label["font"] = widget_label_font
        label.place(x=100, y=200)

