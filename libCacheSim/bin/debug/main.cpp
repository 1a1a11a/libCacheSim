//
// Created by Juncheng Yang on 5/9/21.
//

#include <unistd.h>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../include/libCacheSim/reader.h"

using namespace std;

void load_popularity_data(string input_path, vector<uint32_t>& sorted_freq) {
  ifstream ifs(input_path);
  string line;
  getline(ifs, line);
  getline(ifs, line);

  while (!ifs.eof()) {
    ifs >> line;
    sorted_freq.push_back(stoi(line));
  }

  ifs.close();

  return;
}

int main(int argc, char* argv[]) {


  return 0;
}
