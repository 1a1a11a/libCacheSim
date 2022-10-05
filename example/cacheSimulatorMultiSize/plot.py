

import os, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt 

def _load_data(data_path):
    sizes = []
    miss_ratio_list = []
    with open(data_path) as ifile:
        line = ifile.readline()
        while "cache size" not in line or "num_miss" not in line:
            line = ifile.readline()
            continue

        for line in ifile:
            size, n_miss, n_req, miss_ratio, byte_miss_ratio = line.split()
            sizes.append(int(size))
            miss_ratio_list.append(float(miss_ratio))
    print(sizes, miss_ratio_list)
    return sizes, miss_ratio_list

def plot_data(data_path_lists, names):
    if names is None:
        names = [data_path.split("/")[-1] for data_path in data_path_lists]
    for i in range(len(data_path_lists)):
        data_path = data_path_lists[i]
        sizes, hr_list = _load_data(data_path)
        plt.plot(sizes, hr_list, label=names[i])
    plt.xlabel("Cache size (bytes)", fontsize=20)
    plt.ylabel("Miss ratio", fontsize=20)
    plt.grid(linestyle="--")
    plt.legend(fontsize=20)
    plt.savefig("miss_ratio.png", bbox_inches="tight")
    plt.clf()

if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="plot miss ratio curve") 
    parser.add_argument('--data', type=str, nargs='+', help='data path') 
    parser.add_argument('--name', type=str, nargs='+', default=None, help='names') 

    args = parser.parse_args()
    plot_data(args.data, args.name)







