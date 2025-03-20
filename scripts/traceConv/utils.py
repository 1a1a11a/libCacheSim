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


def categorize_trace(trace_dir, stat_dir):
    """
    Categorize trace files based on the number of objects in the trace

    """

    trace_files = [f for f in os.listdir(trace_dir) if f.endswith(".zst")]
    stat_files = os.listdir(stat_dir)
    assert len(trace_files) == len(stat_files)

    if not os.path.exists(trace_dir + "1k"):
        os.mkdir(trace_dir + "1k")
    if not os.path.exists(trace_dir + "10k"):
        os.mkdir(trace_dir + "10k")
    if not os.path.exists(trace_dir + "100k"):
        os.mkdir(trace_dir + "100k")
    if not os.path.exists(trace_dir + "1m"):
        os.mkdir(trace_dir + "1m")

    for trace_file in trace_files:
        stat_ifile = open(os.path.join(
            stat_dir, trace_file.replace(".zst", ".stat")), "r")
        for line in stat_ifile:
            if line.startswith("n_obj"):
                n_obj = int(line.split(":")[1].strip())
                break
        stat_ifile.close()

        if n_obj < 1000:
            print(f"move {trace_file} to {trace_dir + '/1k'}")
            shutil.move(os.path.join(trace_dir, trace_file),
                        os.path.join(trace_dir + "/1k", trace_file))
        elif n_obj < 10000:
            print(f"move {trace_file} to {trace_dir + '/10k'}")
            shutil.move(os.path.join(trace_dir, trace_file),
                        os.path.join(trace_dir + "/10k", trace_file))
        elif n_obj < 100000:
            print(f"move {trace_file} to {trace_dir + '/100k'}")
            shutil.move(os.path.join(trace_dir, trace_file),
                        os.path.join(trace_dir + "/100k", trace_file))
        elif n_obj < 1000000:
            print(f"move {trace_file} to {trace_dir + '/1m'}")
            shutil.move(os.path.join(trace_dir, trace_file),
                        os.path.join(trace_dir + "/1m", trace_file))
        else:
            print(f"keep {trace_file} in {trace_dir}")

if __name__ == "__main__":
    trace_dir = "/disk/tmp/lcs/"
    stat_dir = "/disk/tmp/stat/"
    categorize_trace(trace_dir, stat_dir)
