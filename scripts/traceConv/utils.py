import os
import subprocess
import shutil
import sys


def post_process(ifilepath, prelcs_path, stat_path, lcs_path):
    dir_path = os.path.dirname(ifilepath)
    if len(dir_path) > 0:
        dir_path += "/"
    if not os.path.exists(dir_path + "stat"):
        os.mkdir(dir_path + "stat")
        os.mkdir(dir_path + "lcs")
        os.mkdir(dir_path + "finished")

    shutil.move(stat_path, f"{dir_path}stat/")

    subprocess.run("zstd -16 --long -T16 " + lcs_path, shell=True)
    shutil.move(f"{lcs_path}.zst", f"{dir_path}lcs/")

    subprocess.run("zstd -8 -T4 " + ifilepath, shell=True)
    os.remove(ifilepath)
    os.remove(prelcs_path)
    os.remove(lcs_path)

    shutil.move(ifilepath + ".zst", f"{dir_path}finished/")
