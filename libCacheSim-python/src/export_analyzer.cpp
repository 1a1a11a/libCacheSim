// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <unordered_map>

#include "../libCacheSim/traceAnalyzer/analyzer.h"
#include "export.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"

namespace libcachesim {

namespace py = pybind11;

// Custom deleters for smart pointers
struct AnalysisParamDeleter {
  void operator()(traceAnalyzer::analysis_param_t* ptr) const {
    if (ptr != nullptr) free(ptr);
  }
};

struct AnalysisOptionDeleter {
  void operator()(traceAnalyzer::analysis_option_t* ptr) const {
    if (ptr != nullptr) free(ptr);
  }
};

void export_analyzer(py::module& m) {
  py::class_<
      traceAnalyzer::analysis_param_t,
      std::unique_ptr<traceAnalyzer::analysis_param_t, AnalysisParamDeleter>>(
      m, "AnalysisParam")
      .def(py::init([](int access_pattern_sample_ratio_inv, int track_n_popular,
                       int track_n_hit, int time_window, int warmup_time) {
             traceAnalyzer::analysis_param_t params;
             params.access_pattern_sample_ratio_inv =
                 access_pattern_sample_ratio_inv;
             params.track_n_popular = track_n_popular;
             params.track_n_hit = track_n_hit;
             params.time_window = time_window;
             params.warmup_time = warmup_time;
             return std::unique_ptr<traceAnalyzer::analysis_param_t,
                                    AnalysisParamDeleter>(
                 new traceAnalyzer::analysis_param_t(params));
           }),
           "access_pattern_sample_ratio_inv"_a = 10, "track_n_popular"_a = 10,
           "track_n_hit"_a = 5, "time_window"_a = 60, "warmup_time"_a = 0)
      .def_readwrite(
          "access_pattern_sample_ratio_inv",
          &traceAnalyzer::analysis_param_t::access_pattern_sample_ratio_inv)
      .def_readwrite("track_n_popular",
                     &traceAnalyzer::analysis_param_t::track_n_popular)
      .def_readwrite("track_n_hit",
                     &traceAnalyzer::analysis_param_t::track_n_hit)
      .def_readwrite("time_window",
                     &traceAnalyzer::analysis_param_t::time_window)
      .def_readwrite("warmup_time",
                     &traceAnalyzer::analysis_param_t::warmup_time);

  py::class_<
      traceAnalyzer::analysis_option_t,
      std::unique_ptr<traceAnalyzer::analysis_option_t, AnalysisOptionDeleter>>(
      m, "AnalysisOption")
      .def(
          py::init([](bool req_rate, bool access_pattern, bool size, bool reuse,
                      bool popularity, bool ttl, bool popularity_decay,
                      bool lifetime, bool create_future_reuse_ccdf,
                      bool prob_at_age, bool size_change) {
            traceAnalyzer::analysis_option_t option;
            option.req_rate = req_rate;
            option.access_pattern = access_pattern;
            option.size = size;
            option.reuse = reuse;
            option.popularity = popularity;
            option.ttl = ttl;
            option.popularity_decay = popularity_decay;
            option.lifetime = lifetime;
            option.create_future_reuse_ccdf = create_future_reuse_ccdf;
            option.prob_at_age = prob_at_age;
            option.size_change = size_change;
            return std::unique_ptr<traceAnalyzer::analysis_option_t,
                                   AnalysisOptionDeleter>(
                new traceAnalyzer::analysis_option_t(option));
          }),
          "req_rate"_a = true, "access_pattern"_a = true, "size"_a = true,
          "reuse"_a = true, "popularity"_a = true, "ttl"_a = false,
          "popularity_decay"_a = false, "lifetime"_a = false,
          "create_future_reuse_ccdf"_a = false, "prob_at_age"_a = false,
          "size_change"_a = false)
      .def_readwrite("req_rate", &traceAnalyzer::analysis_option_t::req_rate)
      .def_readwrite("access_pattern",
                     &traceAnalyzer::analysis_option_t::access_pattern)
      .def_readwrite("size", &traceAnalyzer::analysis_option_t::size)
      .def_readwrite("reuse", &traceAnalyzer::analysis_option_t::reuse)
      .def_readwrite("popularity",
                     &traceAnalyzer::analysis_option_t::popularity)
      .def_readwrite("ttl", &traceAnalyzer::analysis_option_t::ttl)
      .def_readwrite("popularity_decay",
                     &traceAnalyzer::analysis_option_t::popularity_decay)
      .def_readwrite("lifetime", &traceAnalyzer::analysis_option_t::lifetime)
      .def_readwrite(
          "create_future_reuse_ccdf",
          &traceAnalyzer::analysis_option_t::create_future_reuse_ccdf)
      .def_readwrite("prob_at_age",
                     &traceAnalyzer::analysis_option_t::prob_at_age)
      .def_readwrite("size_change",
                     &traceAnalyzer::analysis_option_t::size_change);

  py::class_<traceAnalyzer::TraceAnalyzer,
             std::unique_ptr<traceAnalyzer::TraceAnalyzer>>(m, "Analyzer")
      .def(py::init([](reader_t* reader, std::string output_path,
                       const traceAnalyzer::analysis_option_t& option,
                       const traceAnalyzer::analysis_param_t& param) {
             traceAnalyzer::TraceAnalyzer* analyzer =
                 new traceAnalyzer::TraceAnalyzer(reader, output_path, option,
                                                  param);
             return std::unique_ptr<traceAnalyzer::TraceAnalyzer>(analyzer);
           }),
           "reader"_a, "output_path"_a,
           "option"_a = traceAnalyzer::default_option(),
           "param"_a = traceAnalyzer::default_param())
      .def("run", &traceAnalyzer::TraceAnalyzer::run);
}

}  // namespace libcachesim
