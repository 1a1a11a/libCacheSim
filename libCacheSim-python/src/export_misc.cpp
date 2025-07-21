// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include <pybind11/pybind11.h>

#include "../libCacheSim/bin/traceUtils/internal.hpp"
#include "export.h"

namespace libcachesim {

namespace py = pybind11;

void export_misc(py::module& m) {
  // NOTE(haocheng): Here we provide some convertion functions and utilities
  // - convert_to_oracleGeneral
  // - convert_to_lcs: v1 to v8 (default v1)

  m.def("convert_to_oracleGeneral", &traceConv::convert_to_oracleGeneral,
        "reader"_a, "ofilepath"_a, "output_txt"_a = false,
        "remove_size_change"_a = false);
  m.def("convert_to_lcs", &traceConv::convert_to_lcs, "reader"_a, "ofilepath"_a,
        "output_txt"_a = false, "remove_size_change"_a = false,
        "lcs_ver"_a = 1);
}

}  // namespace libcachesim
