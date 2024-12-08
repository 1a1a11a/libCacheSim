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

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

int main(int argc, char* argv[]) {

  if (argc < 2) {
    printf("Usage: %s <trace_file>\n", argv[0]);
    return 1;
  }

  return 0;
}

#ifdef __cplusplus
}
#endif
