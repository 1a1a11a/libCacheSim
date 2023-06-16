#pragma once

#include <fstream>
#include <iostream>
#include "logging.h"

namespace analysis {

class Heatmap {
  enum heatmapType {
    WORKING_SET_REUSE_HEATMAP = 0,
    SIZE_DISTRIBUTION_HEATMAP,
    REUSE_DISTANCE_HEATMAP,

    INVALID_HEATMAP,
  };

  const std::string heatmap_type_names[INVALID_HEATMAP + 1] = {
      "working_set_reuse_heatmap",
      "size_distribution_heatmap",
      "reuse_distance_heatmap",
      "invalid_heatmap"
  };

 public:
  double **heatval;
  int32_t x_dim, y_dim;
  bool initialized = false;

  Heatmap(int32_t x_dim, int32_t y_dim) {
    set_dim(x_dim, y_dim);
  }

  Heatmap() {};

  void set_dim(int32_t x_dim_, int32_t y_dim_) {
    if (initialized) {
      WARNING("heatmap has been initialized\n");
      return;
    }
    x_dim = x_dim_;
    y_dim = y_dim_;
    heatval = new double *[x_dim_];
    for (int i = 0; i < x_dim; i++) {
      heatval[i] = new double[y_dim_];
      for (int j = 0; j < y_dim; j++) {
        heatval[i][j] = -1;
      }
    }
    initialized = true;
  }

  inline void set_val(int32_t x, int32_t y, double v) {
    heatval[x][y] = v;
  }

  void save_heatmap(std::string ofile_path) {
    std::ofstream ofs(ofile_path, std::ios::out);
    for (int j = 0; j < y_dim; j++) {
      for (int i = 0; i < x_dim; i++) {
        ofs << heatval[i][j] << " ";
      }
      ofs << std::endl;
    }
    ofs.close();
  }

  void load_heatmap(std::string ifile_path) {
    std::ifstream ifs(ifile_path);
    ERROR("not supported\n");
  }

  ~Heatmap() {
    if (!initialized)
      return;
    for (int i = 0; i < x_dim; i++) {
      delete[] heatval[i];
    }
    delete heatval;
  }
};
}// namespace analysis