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
    'figure.figsize': [12, 8]
}
plt.rcParams.update(params)
