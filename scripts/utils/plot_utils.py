import os
import logging
import matplotlib.pyplot as plt

logger_font = logging.getLogger("fontTools")
logger_font.setLevel(logging.WARN)

params = {
    "axes.labelsize": 24,
    "axes.titlesize": 24,
    "font.size": 24,
    "xtick.labelsize": 24,
    "ytick.labelsize": 24,
    "lines.linewidth": 2,
    "legend.fontsize": 24,
    "legend.handlelength": 2,
    "legend.borderaxespad": 0.2,
    "figure.figsize": [12, 8],
}
plt.rcParams.update(params)

FIG_DIR = "figure/"
FIG_TYPE = "png"
if not os.path.exists(FIG_DIR):
    os.makedirs(FIG_DIR)

def get_colors(n):
    COLOR_FIVE = [
        "#f0f9e8",
        "#bae4bc",
        "#7bccc4",
        "#43a2ca",
        "#0868ac",
    ]
    COLOR_FIVE = [
        "#eff3ff",
        "#bdd7e7",
        "#6baed6",
        "#3182bd",
        "#08519c",
    ]
    COLOR_FOUR = ["#ca0020", "#f4a582", "#92c5de", "#0571b0"]
    COLOR_THREE = [
        "#e0f3db",
        "#a8ddb5",
        "#43a2ca",
    ]
    COLOR_TWO = ["#ca0020", "#0571b0"]
    # COLOR_TWO = ["#ef8a62", "#67a9cf"]

    COLOR_SIX = [
        "#b2182b",
        "#ef8a62",
        "#fddbc7",
        "#d1e5f0",
        "#67a9cf",
        "#2166ac",
    ]
    COLOR_SEVEN = [
        "#eff3ff",
        "#c6dbef",
        "#9ecae1",
        "#6baed6",
        "#4292c6",
        "#2171b5",
        "#084594",
    ]

    # COLOR_FOUR = ["#eff3ff", "#bdd7e7", "#6baed6", "#2171b5", ]
    COLOR_THREE = [
        "#fee6ce",
        "#fdae6b",
        "#e6550d",
    ]

    colors = {
        2: COLOR_TWO,
        3: COLOR_THREE,
        4: COLOR_FOUR,
        5: COLOR_FIVE,
        6: COLOR_SIX,
        7: COLOR_SEVEN,
    }
    return colors[n]


def get_linestyles():
    linestyles = ["solid", "dotted", "dashed", "dashdot"]
    return linestyles


def get_markers():
    markers = [
        "o",
        "v",
        "s",
        "p",
        "^",
        "<",
        ">",
        "P",
        "1",
        "2",
        "3",
        "4",
        "+",
        "x",
        "X",
        "d",
        "D",
    ]
    markers = "os<*^p>"

    return markers


def get_hatches():
    patterns = ("-", "+", "x", "\\", "*", "o", "O", ".", "/")
    # patterns = ("", '-', '+', 'x', '\\', '*', 'o', 'O', '.', '/')

    return patterns
