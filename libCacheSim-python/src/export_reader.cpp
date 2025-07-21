// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string_view>

#include "cli_reader_utils.h"
#include "config.h"
#include "export.h"
#include "libCacheSim/enum.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"
#include "mystr.h"

namespace libcachesim {

namespace py = pybind11;

// Custom deleters for smart pointers
struct ReaderDeleter {
  void operator()(reader_t* ptr) const {
    if (ptr != nullptr) close_trace(ptr);
  }
};

struct RequestDeleter {
  void operator()(request_t* ptr) const {
    if (ptr != nullptr) free_request(ptr);
  }
};

struct ReaderInitParamDeleter {
  void operator()(reader_init_param_t* ptr) const {
    if (ptr != nullptr) {
      // Free the strdup'ed string if it exists
      if (ptr->binary_fmt_str != nullptr) {
        free(ptr->binary_fmt_str);
        ptr->binary_fmt_str = nullptr;
      }
      free(ptr);
    }
  }
};

struct SamplerDeleter {
  void operator()(sampler_t* ptr) const {
    if (ptr != nullptr && ptr->free != nullptr) {
      ptr->free(ptr);
    }
  }
};

void export_reader(py::module& m) {
  // Sampler type enumeration
  py::enum_<sampler_type>(m, "SamplerType")
      .value("SPATIAL_SAMPLER", sampler_type::SPATIAL_SAMPLER)
      .value("TEMPORAL_SAMPLER", sampler_type::TEMPORAL_SAMPLER)
      .value("SHARDS_SAMPLER", sampler_type::SHARDS_SAMPLER)
      .value("INVALID_SAMPLER", sampler_type::INVALID_SAMPLER)
      .export_values();

  // Trace type enumeration
  py::enum_<trace_type_e>(m, "TraceType")
      .value("CSV_TRACE", trace_type_e::CSV_TRACE)
      .value("BIN_TRACE", trace_type_e::BIN_TRACE)
      .value("PLAIN_TXT_TRACE", trace_type_e::PLAIN_TXT_TRACE)
      .value("ORACLE_GENERAL_TRACE", trace_type_e::ORACLE_GENERAL_TRACE)
      .value("LCS_TRACE", trace_type_e::LCS_TRACE)
      .value("VSCSI_TRACE", trace_type_e::VSCSI_TRACE)
      .value("TWR_TRACE", trace_type_e::TWR_TRACE)
      .value("TWRNS_TRACE", trace_type_e::TWRNS_TRACE)
      .value("ORACLE_SIM_TWR_TRACE", trace_type_e::ORACLE_SIM_TWR_TRACE)
      .value("ORACLE_SYS_TWR_TRACE", trace_type_e::ORACLE_SYS_TWR_TRACE)
      .value("ORACLE_SIM_TWRNS_TRACE", trace_type_e::ORACLE_SIM_TWRNS_TRACE)
      .value("ORACLE_SYS_TWRNS_TRACE", trace_type_e::ORACLE_SYS_TWRNS_TRACE)
      .value("VALPIN_TRACE", trace_type_e::VALPIN_TRACE)
      .value("UNKNOWN_TRACE", trace_type_e::UNKNOWN_TRACE)
      .export_values();

  py::enum_<read_direction>(m, "ReadDirection")
      .value("READ_FORWARD", read_direction::READ_FORWARD)
      .value("READ_BACKWARD", read_direction::READ_BACKWARD)
      .export_values();

  /**
   * @brief Sampler structure
   */
  py::class_<sampler_t, std::unique_ptr<sampler_t, SamplerDeleter>>(m,
                                                                    "Sampler")
      .def(py::init([](double sample_ratio, enum sampler_type type)
                        -> std::unique_ptr<sampler_t, SamplerDeleter> {
             switch (type) {
               case sampler_type::SPATIAL_SAMPLER:
                 return std::unique_ptr<sampler_t, SamplerDeleter>(
                     create_spatial_sampler(sample_ratio));
               case sampler_type::TEMPORAL_SAMPLER:
                 return std::unique_ptr<sampler_t, SamplerDeleter>(
                     create_temporal_sampler(sample_ratio));
               case sampler_type::SHARDS_SAMPLER:
                 throw std::invalid_argument("SHARDS_SAMPLER is not added");
               case sampler_type::INVALID_SAMPLER:
               default:
                 throw std::invalid_argument("Unknown sampler type");
             }
           }),
           "sample_ratio"_a = 0.1, "type"_a = sampler_type::INVALID_SAMPLER)
      .def_readwrite("sampling_ratio_inv", &sampler_t::sampling_ratio_inv)
      .def_readwrite("sampling_ratio", &sampler_t::sampling_ratio)
      .def_readwrite("sampling_salt", &sampler_t::sampling_salt)
      .def_readwrite("sampling_type", &sampler_t::type);

  // Reader initialization parameters
  py::class_<reader_init_param_t>(m, "ReaderInitParam")
      .def(py::init([]() { return default_reader_init_params(); }))
      .def(py::init([](const std::string& binary_fmt_str, bool ignore_obj_size,
                       bool ignore_size_zero_req, bool obj_id_is_num,
                       bool obj_id_is_num_set, int64_t cap_at_n_req,
                       int64_t block_size, bool has_header, bool has_header_set,
                       const std::string& delimiter, ssize_t trace_start_offset,
                       sampler_t* sampler) {
             reader_init_param_t params = default_reader_init_params();

             // Safe string handling with proper error checking
             if (!binary_fmt_str.empty()) {
               char* fmt_str = strdup(binary_fmt_str.c_str());
               if (!fmt_str) {
                 throw std::bad_alloc();
               }
               params.binary_fmt_str = fmt_str;
             }

             params.ignore_obj_size = ignore_obj_size;
             params.ignore_size_zero_req = ignore_size_zero_req;
             params.obj_id_is_num = obj_id_is_num;
             params.obj_id_is_num_set = obj_id_is_num_set;
             params.cap_at_n_req = cap_at_n_req;
             params.block_size = block_size;
             params.has_header = has_header;
             params.has_header_set = has_header_set;
             params.delimiter = delimiter.empty() ? ',' : delimiter[0];
             params.trace_start_offset = trace_start_offset;
             params.sampler = sampler;
             return params;
           }),
           "binary_fmt_str"_a = "", "ignore_obj_size"_a = false,
           "ignore_size_zero_req"_a = true, "obj_id_is_num"_a = true,
           "obj_id_is_num_set"_a = false, "cap_at_n_req"_a = -1,
           "block_size"_a = -1, "has_header"_a = false,
           "has_header_set"_a = false, "delimiter"_a = ",",
           "trace_start_offset"_a = 0, "sampler"_a = nullptr)
      .def_readwrite("ignore_obj_size", &reader_init_param_t::ignore_obj_size)
      .def_readwrite("ignore_size_zero_req",
                     &reader_init_param_t::ignore_size_zero_req)
      .def_readwrite("obj_id_is_num", &reader_init_param_t::obj_id_is_num)
      .def_readwrite("obj_id_is_num_set",
                     &reader_init_param_t::obj_id_is_num_set)
      .def_readwrite("cap_at_n_req", &reader_init_param_t::cap_at_n_req)
      .def_readwrite("time_field", &reader_init_param_t::time_field)
      .def_readwrite("obj_id_field", &reader_init_param_t::obj_id_field)
      .def_readwrite("obj_size_field", &reader_init_param_t::obj_size_field)
      .def_readwrite("op_field", &reader_init_param_t::op_field)
      .def_readwrite("ttl_field", &reader_init_param_t::ttl_field)
      .def_readwrite("cnt_field", &reader_init_param_t::cnt_field)
      .def_readwrite("tenant_field", &reader_init_param_t::tenant_field)
      .def_readwrite("next_access_vtime_field",
                     &reader_init_param_t::next_access_vtime_field)
      .def_readwrite("n_feature_fields", &reader_init_param_t::n_feature_fields)
      // .def_readwrite("feature_fields", &reader_init_param_t::feature_fields)
      .def_property(
          "feature_fields",
          [](const reader_init_param_t& self) {
            return py::array_t<int>({self.n_feature_fields},
                                    self.feature_fields);  // copy to python
          },
          [](reader_init_param_t& self, py::array_t<int> arr) {
            if (arr.size() != self.n_feature_fields)
              throw std::runtime_error("Expected array of size " +
                                       std::to_string(self.n_feature_fields));
            std::memcpy(
                self.feature_fields, arr.data(),
                self.n_feature_fields * sizeof(int));  // write to C++ array
          })
      .def_readwrite("block_size", &reader_init_param_t::block_size)
      .def_readwrite("has_header", &reader_init_param_t::has_header)
      .def_readwrite("has_header_set", &reader_init_param_t::has_header_set)
      .def_readwrite("delimiter", &reader_init_param_t::delimiter)
      .def_readwrite("trace_start_offset",
                     &reader_init_param_t::trace_start_offset)
      .def_readwrite("binary_fmt_str", &reader_init_param_t::binary_fmt_str)
      .def_readwrite("sampler", &reader_init_param_t::sampler);

  /**
   * @brief Reader structure
   */
  py::class_<reader_t, std::unique_ptr<reader_t, ReaderDeleter>>(m, "Reader")
      .def(py::init([](const std::string& trace_path, trace_type_e trace_type,
                       const reader_init_param_t& init_params) {
             trace_type_e final_trace_type = trace_type;
             if (final_trace_type == trace_type_e::UNKNOWN_TRACE) {
               final_trace_type = detect_trace_type(trace_path.c_str());
             }
             reader_t* ptr = setup_reader(trace_path.c_str(), final_trace_type,
                                          &init_params);
             if (ptr == nullptr) {
               throw std::runtime_error("Failed to create reader for " +
                                        trace_path);
             }
             return std::unique_ptr<reader_t, ReaderDeleter>(ptr);
           }),
           "trace_path"_a, "trace_type"_a = trace_type_e::UNKNOWN_TRACE,
           "init_params"_a = default_reader_init_params())
      .def_readonly("n_read_req", &reader_t::n_read_req)
      .def_readonly("n_total_req", &reader_t::n_total_req)
      .def_readonly("trace_path", &reader_t::trace_path)
      .def_readonly("file_size", &reader_t::file_size)
      .def_readonly("init_params", &reader_t::init_params)
      .def_readonly("trace_type", &reader_t::trace_type)
      .def_readonly("trace_format", &reader_t::trace_format)
      .def_readonly("ver", &reader_t::ver)
      .def_readonly("cloned", &reader_t::cloned)
      .def_readonly("cap_at_n_req", &reader_t::cap_at_n_req)
      .def_readonly("trace_start_offset", &reader_t::trace_start_offset)
      // For binary traces
      .def_readonly("mapped_file", &reader_t::mapped_file)
      .def_readonly("mmap_offset", &reader_t::mmap_offset)
      // .def_readonly("zstd_reader_p", &reader_t::zstd_reader_p)
      .def_readonly("is_zstd_file", &reader_t::is_zstd_file)
      .def_readonly("item_size", &reader_t::item_size)
      // For text traces
      .def_readonly("file", &reader_t::file)
      .def_readonly("line_buf", &reader_t::line_buf)
      .def_readonly("line_buf_size", &reader_t::line_buf_size)
      .def_readonly("csv_delimiter", &reader_t::csv_delimiter)
      .def_readonly("csv_has_header", &reader_t::csv_has_header)
      .def_readonly("obj_id_is_num", &reader_t::obj_id_is_num)
      .def_readonly("obj_id_is_num_set", &reader_t::obj_id_is_num_set)
      // Other properties
      .def_readwrite("ignore_size_zero_req", &reader_t::ignore_size_zero_req)
      .def_readwrite("ignore_obj_size", &reader_t::ignore_obj_size)
      .def_readwrite("block_size", &reader_t::block_size)
      .def_readonly("n_req_left", &reader_t::n_req_left)
      .def_readonly("last_req_clock_time", &reader_t::last_req_clock_time)
      .def_readonly("lcs_ver", &reader_t::lcs_ver)
      // TODO(haocheng): Fully support sampler in Python bindings
      .def_readonly("sampler", &reader_t::sampler)
      .def_readonly("read_direction", &reader_t::read_direction)
      .def("get_num_of_req",
           [](reader_t& self) { return get_num_of_req(&self); })
      .def(
          "read_one_req",
          [](reader_t& self, request_t& req) {
            int ret = read_one_req(&self, &req);
            if (ret != 0) {
              throw std::runtime_error("Failed to read request");
            }
            return req;
          },
          "req"_a)
      .def("reset", [](reader_t& self) { reset_reader(&self); })
      .def("close", [](reader_t& self) { close_reader(&self); })
      .def("clone",
           [](const reader_t& self) {
             reader_t* cloned_reader = clone_reader(&self);
             if (cloned_reader == nullptr) {
               throw std::runtime_error("Failed to clone reader");
             }
             return std::unique_ptr<reader_t, ReaderDeleter>(cloned_reader);
           })
      .def(
          "read_first_req",
          [](reader_t& self, request_t& req) {
            read_first_req(&self, &req);
            return req;
          },
          "req"_a)
      .def(
          "read_last_req",
          [](reader_t& self, request_t& req) {
            read_last_req(&self, &req);
            return req;
          },
          "req"_a)
      .def(
          "skip_n_req",
          [](reader_t& self, int n) {
            int ret = skip_n_req(&self, n);
            if (ret != 0) {
              throw std::runtime_error("Failed to skip requests");
            }
            return ret;
          },
          "n"_a)
      .def("read_one_req_above",
           [](reader_t& self) {
             request_t* req = new_request();
             int ret = read_one_req_above(&self, req);
             if (ret != 0) {
               free_request(req);
               throw std::runtime_error("Failed to read one request above");
             }
             return std::unique_ptr<request_t, RequestDeleter>(req);
           })
      .def("go_back_one_req",
           [](reader_t& self) {
             int ret = go_back_one_req(&self);
             if (ret != 0) {
               throw std::runtime_error("Failed to go back one request");
             }
           })
      .def(
          "set_read_pos",
          [](reader_t& self, double pos) { reader_set_read_pos(&self, pos); },
          "pos"_a);
}
}  // namespace libcachesim
