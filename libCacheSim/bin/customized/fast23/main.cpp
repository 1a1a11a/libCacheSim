

#include <unistd.h>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "fast23.h"

using namespace std;

void print_help(char *argv[]) {
    /* only support oracleGeneral trace */
    cout << "Usage: " << argv[0] << " mode[groups/grouping] trace_path ofilepath [group_size/n_repeat] [n_thread]" << endl;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        print_help(argv);
        return 0;
    }

    string mode = string(argv[1]);
    string trace_path = string(argv[2]);
    string ofilepath = string(argv[3]);

    if (mode == "groups") {
        /* calculate the reuse time of groups */
        int group_size = 100;
        if (argc > 4) {
            group_size = atoi(argv[4]);
        }
        // compareGroups::cal_group_metric_over_time(trace_path, ofilepath, group_size);
        compareGroups::cal_group_metric_utility_over_time(trace_path, ofilepath, group_size);
    } else if (mode == "grouping") {
        // compare time-based grouping and random grouping
        vector<int> group_sizes{10, 20, 40, 80, 160, 320,
                                640, 1280, 2560, 5120, 10240, 20480}; 
                                // ,
                                // 40960, 81920, 163840, 327680};
        int n_repeat = 10000;
        double drop_frac = 0.0;// drop the top drop_frac of popular objects
        int n_thread = std::thread::hardware_concurrency();
        if (argc > 4) n_repeat = atoi(argv[4]);
        if (argc > 5) n_thread = atoi(argv[5]);
        // compareGrouping::cal_group_metric(trace_path, ofilepath, group_sizes, n_repeat, drop_frac, n_thread);
        compareGrouping::cal_group_metric_utility(trace_path, ofilepath, group_sizes, n_repeat, n_thread);
    } else {
        print_help(argv);
        return 0;
    }

    return 0;
}
