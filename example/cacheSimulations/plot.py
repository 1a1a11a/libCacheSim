

import os, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt 

def _load_data(data_path):
    sizes = []
    hr_list = []
    with open(data_path) as ifile:
        ifile.readline()
        for line in ifile:
            size, n_req, n_hit, hr = line.split()
            sizes.append(int(size))
            hr_list.append(float(hr))
    return sizes, hr_list 

def plot_data(data_path_lists, names):
    if names is None:
        names = [data_path.split("/")[-1] for data_path in data_path_lists]
    for i in range(len(data_path_lists)):
        data_path = data_path_lists[i]
        sizes, hr_list = _load_data(data_path)
        plt.plot(sizes, hr_list, label=names[i])
    plt.xlabel("Cache size (object)", fontsize=20) 
    plt.ylabel("Hit ratio", fontsize=20)
    plt.grid(linestyle="--")
    plt.legend(fontsize=20)
    plt.savefig("hit_ratio.pdf", bbox_inches="tight")
    plt.clf()

if __name__ == "__main__":
    import argparse 
    parser = argparse.ArgumentParser(description="plot hit ratio curve") 
    parser.add_argument('--data', type=str, nargs='+', help='data path') 
    parser.add_argument('--name', type=str, nargs='+', default=None, help='names') 

    args = parser.parse_args()
    plot_data(args.data, args.name)







