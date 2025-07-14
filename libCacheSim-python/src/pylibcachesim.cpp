#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// Suppress visibility warnings for pybind11 types
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

#include <iostream>
#include <memory>
#include <unordered_map>

#include "config.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/const.h"
#include "libCacheSim/enum.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/macro.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"
#include "libCacheSim/sampling.h"
#include "mystr.h"

/* admission */
#include "libCacheSim/admissionAlgo.h"

/* eviction */
#include "libCacheSim/evictionAlgo.h"

/* cache simulator */
#include "libCacheSim/profilerLRU.h"
#include "libCacheSim/simulator.h"

/* bin */
#include "cachesim/cache_init.h"
#include "cli_reader_utils.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

// Helper functions

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
static bool ends_with(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

trace_type_e infer_trace_type(const std::string& trace_path) {
  // Infer the trace type based on the file extension
  if (trace_path.find("oracleGeneral") != std::string::npos) {
    return trace_type_e::ORACLE_GENERAL_TRACE;
  } else if (ends_with(trace_path, ".csv")) {
    return trace_type_e::CSV_TRACE;
  } else if (ends_with(trace_path, ".txt")) {
    return trace_type_e::PLAIN_TXT_TRACE;
  } else if (ends_with(trace_path, ".bin")) {
    return trace_type_e::BIN_TRACE;
  } else if (ends_with(trace_path, ".vscsi")) {
    return trace_type_e::VSCSI_TRACE;
  } else if (ends_with(trace_path, ".twr")) {
    return trace_type_e::TWR_TRACE;
  } else if (ends_with(trace_path, ".twrns")) {
    return trace_type_e::TWRNS_TRACE;
  } else if (ends_with(trace_path, ".lcs")) {
    return trace_type_e::LCS_TRACE;
  } else if (ends_with(trace_path, ".valpin")) {
    return trace_type_e::VALPIN_TRACE;
  } else {
    return trace_type_e::UNKNOWN_TRACE;
  }
}

// Python Hook Cache Implementation
class PythonHookCache {
 private:
  uint64_t cache_size_;
  std::string cache_name_;
  std::unordered_map<uint64_t, uint64_t> objects_;  // obj_id -> obj_size
  py::object plugin_data_;

  // Hook functions
  py::function init_hook_;
  py::function hit_hook_;
  py::function miss_hook_;
  py::function eviction_hook_;
  py::function remove_hook_;
  py::object free_hook_;  // Changed to py::object to allow py::none()

 public:
  uint64_t n_req = 0;
  uint64_t n_obj = 0;
  uint64_t occupied_byte = 0;
  uint64_t cache_size;

  PythonHookCache(uint64_t cache_size,
                  const std::string& cache_name = "PythonHookCache")
      : cache_size_(cache_size),
        cache_name_(cache_name),
        cache_size(cache_size),
        free_hook_(py::none()) {}

  void set_hooks(py::function init_hook, py::function hit_hook,
                 py::function miss_hook, py::function eviction_hook,
                 py::function remove_hook, py::object free_hook = py::none()) {
    init_hook_ = init_hook;
    hit_hook_ = hit_hook;
    miss_hook_ = miss_hook;
    eviction_hook_ = eviction_hook;
    remove_hook_ = remove_hook;

    // Handle free_hook properly
    if (!free_hook.is_none()) {
      free_hook_ = free_hook;
    } else {
      free_hook_ = py::none();
    }

    // Initialize plugin data
    plugin_data_ = init_hook_(cache_size_);
  }

  bool get(const request_t& req) {
    n_req++;

    auto it = objects_.find(req.obj_id);
    if (it != objects_.end()) {
      // Cache hit
      hit_hook_(plugin_data_, req.obj_id, req.obj_size);
      return true;
    } else {
      // Cache miss - call miss hook first
      miss_hook_(plugin_data_, req.obj_id, req.obj_size);

      // Check if eviction is needed
      while (occupied_byte + req.obj_size > cache_size_ && !objects_.empty()) {
        // Need to evict
        uint64_t victim_id =
            eviction_hook_(plugin_data_, req.obj_id, req.obj_size)
                .cast<uint64_t>();
        auto victim_it = objects_.find(victim_id);
        if (victim_it != objects_.end()) {
          occupied_byte -= victim_it->second;
          objects_.erase(victim_it);
          n_obj--;
          remove_hook_(plugin_data_, victim_id);
        } else {
          // Safety check: if eviction hook returns invalid ID, break to avoid
          // infinite loop
          break;
        }
      }

      // Insert new object if there's space
      if (occupied_byte + req.obj_size <= cache_size_) {
        objects_[req.obj_id] = req.obj_size;
        occupied_byte += req.obj_size;
        n_obj++;
      }

      return false;
    }
  }

  ~PythonHookCache() {
    if (!free_hook_.is_none()) {
      py::function free_func = free_hook_.cast<py::function>();
      free_func(plugin_data_);
    }
  }
};

// Restore visibility warnings
#pragma GCC diagnostic pop

struct CacheDeleter {
  void operator()(cache_t* ptr) const {
    if (ptr != nullptr) ptr->cache_free(ptr);
  }
};

struct RequestDeleter {
  void operator()(request_t* ptr) const {
    if (ptr != nullptr) free_request(ptr);
  }
};

struct ReaderDeleter {
  void operator()(reader_t* ptr) const {
    if (ptr != nullptr) close_trace(ptr);
  }
};

PYBIND11_MODULE(_libcachesim, m) {  // NOLINT(readability-named-parameter)
  m.doc() = R"pbdoc(
        libCacheSim Python bindings
        --------------------------

        .. currentmodule:: libcachesim

        .. autosummary::
           :toctree: _generate

           TODO(haocheng): add meaningful methods
    )pbdoc";

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

  // *************** structs ***************
  /**
   * @brief Cache structure
   */
  py::class_<cache_t, std::unique_ptr<cache_t, CacheDeleter>>(m, "Cache")
      .def_readwrite("n_req", &cache_t::n_req)
      .def_readwrite("n_obj", &cache_t::n_obj)
      .def_readwrite("occupied_byte", &cache_t::occupied_byte)
      .def_readwrite("cache_size", &cache_t::cache_size)
      // methods
      .def("get", [](cache_t& self, const request_t& req) {
        return self.get(&self, &req);
      });

  /**
   * @brief Request structure
   */
  py::class_<request_t, std::unique_ptr<request_t, RequestDeleter>>(m,
                                                                    "Request")
      .def(py::init([]() { return new_request(); }))
      .def_readwrite("clock_time", &request_t::clock_time)
      .def_readwrite("hv", &request_t::hv)
      .def_readwrite("obj_id", &request_t::obj_id)
      .def_readwrite("obj_size", &request_t::obj_size)
      .def_readwrite("op", &request_t::op);

  /**
   * @brief Reader structure
   */
  py::class_<reader_t, std::unique_ptr<reader_t, ReaderDeleter>>(m, "Reader")
      .def_readwrite("n_read_req", &reader_t::n_read_req)
      .def_readwrite("n_total_req", &reader_t::n_total_req)
      .def_readwrite("trace_path", &reader_t::trace_path)
      .def_readwrite("file_size", &reader_t::file_size)
      // methods
      .def(
          "get_wss",
          [](reader_t& self, bool ignore_obj_size) {
            int64_t wss_obj = 0, wss_byte = 0;
            cal_working_set_size(&self, &wss_obj, &wss_byte);
            return ignore_obj_size ? wss_obj : wss_byte;
          },
          py::arg("ignore_obj_size") = false,
          R"pbdoc(
            Get the working set size of the trace.

            Args:
                ignore_obj_size (bool): Whether to ignore the object size.

            Returns:
                int: The working set size of the trace.
      )pbdoc")
      .def(
          "seek",
          [](reader_t& self, int64_t offset, bool from_beginning = false) {
            int64_t offset_from_beginning = offset;
            if (!from_beginning) {
              offset_from_beginning += self.n_read_req;
            }
            reset_reader(&self);
            skip_n_req(&self, offset_from_beginning);
          },
          py::arg("offset"), py::arg("from_beginning") = false,
          R"pbdoc(
            Seek to a specific offset in the trace file.
            We only support seeking from current position or from the beginning.

            Can only move forward, not backward.

            Args:
                offset (int): The offset to seek to the beginning.

            Raises:
                RuntimeError: If seeking fails.
      )pbdoc")
      .def("__iter__", [](reader_t& self) -> reader_t& { return self; })
      .def("__next__", [](reader_t& self) {
        auto req = std::unique_ptr<request_t, RequestDeleter>(new_request());
        int ret = read_one_req(&self, req.get());
        if (ret != 0) {
          throw py::stop_iteration();
        }
        // std::cout << "Read request: " << req->obj_id
        //           << ", size: " << req->obj_size << std::endl;
        return req;
      });

  py::class_<reader_init_param_t>(m, "reader_init_param_t")
      .def(py::init<>())
      .def_readwrite("time_field", &reader_init_param_t::time_field)
      .def_readwrite("obj_id_field", &reader_init_param_t::obj_id_field)
      .def_readwrite("obj_size_field", &reader_init_param_t::obj_size_field)
      .def_readwrite("delimiter", &reader_init_param_t::delimiter)
      .def_readwrite("has_header", &reader_init_param_t::has_header)
      .def_property(
          "binary_fmt_str",
          // Getter: C char* to Python string (returns copy)
          [](const reader_init_param_t& self) {
            return self.binary_fmt_str ? std::string(self.binary_fmt_str) : "";
          },
          // Setter: Python string to C char* (handles deep copy and old memory)
          [](reader_init_param_t& self, const std::string& value) {
            // Free existing memory if any
            if (self.binary_fmt_str != nullptr) {
              free(self.binary_fmt_str);  // Use free() since it was
                                          // strdup'd/malloc'd
            }
            // Deep copy the new string
            self.binary_fmt_str = strdup(value.c_str());
            if (self.binary_fmt_str == nullptr && !value.empty()) {
              throw std::runtime_error(
                  "Failed to allocate memory for binary_fmt_str");
            }
          });

  // *************** functions ***************
  /**
   * @brief Open a trace file for reading
   */
  m.def(
      "open_trace",
      [](const std::string& trace_path, py::object type, py::object params) {
        trace_type_e c_type = UNKNOWN_TRACE;
        if (!type.is_none()) {
          c_type = type.cast<trace_type_e>();
        } else {
          // If type is None, we can try to infer the type from the file
          // extension
          c_type = infer_trace_type(trace_path);
          if (c_type == UNKNOWN_TRACE) {
            throw std::runtime_error("Could not infer trace type from path: " +
                                     trace_path);
          }
        }

        // Create an init_param instance, it will be populated from Python
        reader_init_param_t init_param = {};

        // === IMPORTANT: Initialize binary_fmt_str to nullptr ===
        // This is crucial if it's not always set from Python,
        // so that free() won't be called on uninitialized memory if not set
        // later.
        init_param.binary_fmt_str = nullptr;

        // Populate other fields from Python dict or object
        if (py::isinstance<py::dict>(params)) {
          py::dict dict_params = params.cast<py::dict>();
          init_param.time_field = dict_params["time_field"].cast<int>();
          init_param.obj_id_field = dict_params["obj_id_field"].cast<int>();
          init_param.obj_size_field = dict_params["obj_size_field"].cast<int>();
          init_param.delimiter =
              dict_params["delimiter"].cast<std::string>()[0];
          init_param.has_header = dict_params["has_header"].cast<bool>();
          // If binary_fmt_str is in dict_params, set it via property setter
          if (dict_params.contains("binary_fmt_str") &&
              !dict_params["binary_fmt_str"].is_none()) {
            std::string bfs_val =
                dict_params["binary_fmt_str"].cast<std::string>();
            if (init_param.binary_fmt_str != nullptr)
              free(init_param.binary_fmt_str);
            init_param.binary_fmt_str = strdup(bfs_val.c_str());
            if (init_param.binary_fmt_str == nullptr && !bfs_val.empty()) {
              throw std::runtime_error(
                  "Failed to allocate memory for binary_fmt_str from dict");
            }
          }
        } else if (!params.is_none()) {
          // If using a reader_init_param_t object from Python, its members are
          // already set via def_property (No need to copy here, just ensure
          // it's reader_init_param_t object) If `params` is a
          // `reader_init_param_t` object, Pybind11 will pass its fields
          // directly We need to ensure that the `binary_fmt_str` member of
          // `params` is correctly handled. The direct `getattr` below is for
          // other fields, for binary_fmt_str, the `def_property` takes care.
          init_param.time_field = py::getattr(params, "time_field").cast<int>();
          init_param.obj_id_field =
              py::getattr(params, "obj_id_field").cast<int>();
          init_param.obj_size_field =
              py::getattr(params, "obj_size_field").cast<int>();
          init_param.delimiter =
              py::getattr(params, "delimiter").cast<std::string>()[0];
          init_param.has_header =
              py::getattr(params, "has_header").cast<bool>();
          // Handle binary_fmt_str if it's set on the Python object
          if (py::hasattr(params, "binary_fmt_str") &&
              !py::getattr(params, "binary_fmt_str").is_none()) {
            std::string bfs_val =
                py::getattr(params, "binary_fmt_str").cast<std::string>();
            if (init_param.binary_fmt_str != nullptr)
              free(init_param.binary_fmt_str);
            init_param.binary_fmt_str = strdup(bfs_val.c_str());
            if (init_param.binary_fmt_str == nullptr && !bfs_val.empty()) {
              throw std::runtime_error(
                  "Failed to allocate memory for binary_fmt_str from object");
            }
          }
        }
        reader_t* ptr = open_trace(trace_path.c_str(), c_type, &init_param);
        return std::unique_ptr<reader_t, ReaderDeleter>(ptr);
      },
      py::arg("trace_path"), py::arg("type") = py::none(),
      py::arg("params") = py::none(),
      R"pbdoc(
            Open a trace file for reading.

            Args:
                trace_path (str): Path to the trace file.
                type (Union[trace_type_e, None]): Type of the trace (e.g., CSV_TRACE). If None, the type will be inferred.
                params (Union[dict, reader_init_param_t, None]): Initialization parameters for the reader.

            Returns:
                Reader: A new reader instance for the trace.
        )pbdoc");

  /**
   * @brief Generic function to create a cache instance.
   */
  m.def(
      "create_cache",
      [](const std::string& eviction_algo, const uint64_t cache_size,
         const std::string& eviction_params,
         bool consider_obj_metadata) { return nullptr; },
      py::arg("eviction_algo"), py::arg("cache_size"),
      py::arg("eviction_params"), py::arg("consider_obj_metadata"),
      R"pbdoc(
            Create a cache instance.

            Args:
                eviction_algo (str): Eviction algorithm to use (e.g., "LRU", "FIFO", "Random").
                cache_size (int): Size of the cache in bytes.
                eviction_params (str): Additional parameters for the eviction algorithm.
                consider_obj_metadata (bool): Whether to consider object metadata in eviction decisions.

            Returns:
                Cache: A new cache instance.
        )pbdoc");

  /* TODO(haocheng): should we support all parameters in the
   * common_cache_params_t? (hash_power, etc.) */

  // Currently supported eviction algorithms with direct initialization:
  //   - "ARC"
  //   - "Clock"
  //   - "FIFO"
  //   - "LRB"
  //   - "LRU"
  //   - "S3FIFO"
  //   - "Sieve"
  //   - "ThreeLCache"
  //   - "TinyLFU"
  //   - "TwoQ"

  /**
   * @brief Create a ARC cache instance.
   */
  m.def(
      "ARC_init",
      [](uint64_t cache_size) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = ARC_init(cc_params, nullptr);
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"),
      R"pbdoc(
            Create a ARC cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
      )pbdoc");

  /**
   * @brief Create a Clock cache instance.
   */
  m.def(
      "Clock_init",
      [](uint64_t cache_size, long int n_bit_counter, long int init_freq) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        // assemble the cache specific parameters
        std::string cache_specific_params =
            "n-bit-counter=" + std::to_string(n_bit_counter) + "," +
            "init-freq=" + std::to_string(init_freq);

        cache_t* ptr = Clock_init(cc_params, cache_specific_params.c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("n_bit_counter") = 1,
      py::arg("init_freq") = 0,
      R"pbdoc(
            Create a Clock cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                n_bit_counter (int): Number of bits for counter (default: 1).
                init_freq (int): Initial frequency value (default: 0).

            Returns:
                Cache: A new Clock cache instance.
      )pbdoc");

  /**
   * @brief Create a FIFO cache instance.
   */
  m.def(
      "FIFO_init",
      [](uint64_t cache_size) {
        // Construct common cache parameters
        common_cache_params_t cc_params = {.cache_size = cache_size};
        // FIFO no specific parameters, so we pass nullptr
        cache_t* ptr = FIFO_init(cc_params, nullptr);
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"),
      R"pbdoc(
            Create a FIFO cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.

            Returns:
                Cache: A new FIFO cache instance.
      )pbdoc");

#ifdef ENABLE_LRB
  /**
   * @brief Create a LRB cache instance.
   */
  m.def(
      "LRB_init",
      [](uint64_t cache_size, std::string objective) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = LRB_init(cc_params, ("objective=" + objective).c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("objective") = "byte-miss-ratio",
      R"pbdoc(
            Create a LRB cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                objective (str): Objective function to optimize (default: "byte-miss-ratio").

            Returns:
                Cache: A new LRB cache instance.
      )pbdoc");
#else
  // TODO(haocheng): add a dummy function to avoid the error when LRB is not
  // enabled
  m.def(
      "LRB_init",
      [](uint64_t cache_size, std::string objective) {
        throw std::runtime_error("LRB is not enabled");
      },
      py::arg("cache_size"), py::arg("objective") = "byte-miss-ratio");
#endif

  /**
   * @brief Create a LRU cache instance.
   */
  m.def(
      "LRU_init",
      [](uint64_t cache_size) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = LRU_init(cc_params, nullptr);
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"),
      R"pbdoc(
            Create a LRU cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.

            Returns:
                Cache: A new LRU cache instance.
      )pbdoc");

  /**
   * @brief Create a S3FIFO cache instance.
   */
  m.def(
      "S3FIFO_init",
      [](uint64_t cache_size, double fifo_size_ratio, double ghost_size_ratio,
         int move_to_main_threshold) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = S3FIFO_init(
            cc_params,
            ("fifo-size-ratio=" + std::to_string(fifo_size_ratio) + "," +
             "ghost-size-ratio=" + std::to_string(ghost_size_ratio) + "," +
             "move-to-main-threshold=" + std::to_string(move_to_main_threshold))
                .c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("fifo_size_ratio") = 0.10,
      py::arg("ghost_size_ratio") = 0.90, py::arg("move_to_main_threshold") = 2,
      R"pbdoc(
            Create a S3FIFO cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                fifo_size_ratio (float): Ratio of FIFO size to cache size (default: 0.10).
                ghost_size_ratio (float): Ratio of ghost size to cache size (default: 0.90).
                move_to_main_threshold (int): Threshold for moving to main queue (default: 2).

            Returns:
                Cache: A new S3FIFO cache instance.
      )pbdoc");

  /**
   * @brief Create a Sieve cache instance.
   */
  m.def(
      "Sieve_init",
      [](uint64_t cache_size) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = Sieve_init(cc_params, nullptr);
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"),
      R"pbdoc(
            Create a Sieve cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.

            Returns:
                Cache: A new Sieve cache instance.
      )pbdoc");

#ifdef ENABLE_3L_CACHE
  /**
   * @brief Create a ThreeL cache instance.
   */
  m.def(
      "ThreeLCache_init",
      [](uint64_t cache_size, std::string objective) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr =
            ThreeLCache_init(cc_params, ("objective=" + objective).c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("objective") = "byte-miss-ratio",
      R"pbdoc(
            Create a ThreeL cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                objective (str): Objective function to optimize (default: "byte-miss-ratio").

            Returns:
                Cache: A new ThreeL cache instance.
      )pbdoc");
#else
  // TODO(haocheng): add a dummy function to avoid the error when ThreeLCache is
  // not enabled
  m.def(
      "ThreeLCache_init",
      [](uint64_t cache_size, std::string objective) {
        throw std::runtime_error("ThreeLCache is not enabled");
      },
      py::arg("cache_size"), py::arg("objective") = "byte-miss-ratio");
#endif

  /**
   * @brief Create a TinyLFU cache instance.
   */
  // mark evivtion parsing need change
  m.def(
      "TinyLFU_init",
      [](uint64_t cache_size, std::string main_cache, double window_size) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = WTinyLFU_init(
            cc_params, ("main-cache=" + main_cache + "," +
                        "window-size=" + std::to_string(window_size))
                           .c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("main_cache") = "SLRU",
      py::arg("window_size") = 0.01,
      R"pbdoc(
            Create a TinyLFU cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                main_cache (str): Main cache to use (default: "SLRU").
                window_size (float): Window size for TinyLFU (default: 0.01).

            Returns:
                Cache: A new TinyLFU cache instance.
      )pbdoc");

  /**
   * @brief Create a TwoQ cache instance.
   */
  m.def(
      "TwoQ_init",
      [](uint64_t cache_size, double Ain_size_ratio, double Aout_size_ratio) {
        common_cache_params_t cc_params = {.cache_size = cache_size};
        cache_t* ptr = TwoQ_init(
            cc_params,
            ("Ain-size-ratio=" + std::to_string(Ain_size_ratio) + "," +
             "Aout-size-ratio=" + std::to_string(Aout_size_ratio))
                .c_str());
        return std::unique_ptr<cache_t, CacheDeleter>(ptr);
      },
      py::arg("cache_size"), py::arg("Ain_size_ratio") = 0.25,
      py::arg("Aout_size_ratio") = 0.5,
      R"pbdoc(
            Create a TwoQ cache instance.

            Args:
                cache_size (int): Size of the cache in bytes.
                Ain_size_ratio (float): Ratio of A-in size to cache size (default: 0.25).
                Aout_size_ratio (float): Ratio of A-out size to cache size (default: 0.5).

            Returns:
                Cache: A new TwoQ cache instance.
      )pbdoc");

  /**
   * @brief Create a Python hook-based cache instance.
   */
  py::class_<PythonHookCache>(m, "PythonHookCache")
      .def(py::init<uint64_t, const std::string&>(), py::arg("cache_size"),
           py::arg("cache_name") = "PythonHookCache")
      .def("set_hooks", &PythonHookCache::set_hooks, py::arg("init_hook"),
           py::arg("hit_hook"), py::arg("miss_hook"), py::arg("eviction_hook"),
           py::arg("remove_hook"), py::arg("free_hook") = py::none(),
           R"pbdoc(
            Set the hook functions for the cache.

            Args:
                init_hook (callable): Function called during cache initialization.
                    Signature: init_hook(cache_size: int) -> Any
                hit_hook (callable): Function called on cache hit.
                    Signature: hit_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None
                miss_hook (callable): Function called on cache miss.
                    Signature: miss_hook(plugin_data: Any, obj_id: int, obj_size: int) -> None
                eviction_hook (callable): Function called to select eviction candidate.
                    Signature: eviction_hook(plugin_data: Any, obj_id: int, obj_size: int) -> int
                remove_hook (callable): Function called when object is removed.
                    Signature: remove_hook(plugin_data: Any, obj_id: int) -> None
                free_hook (callable, optional): Function called during cache cleanup.
                    Signature: free_hook(plugin_data: Any) -> None
      )pbdoc")
      .def("get", &PythonHookCache::get, py::arg("req"),
           R"pbdoc(
            Process a cache request.

            Args:
                req (Request): The cache request to process.

            Returns:
                bool: True if cache hit, False if cache miss.
      )pbdoc")
      .def_readwrite("n_req", &PythonHookCache::n_req)
      .def_readwrite("n_obj", &PythonHookCache::n_obj)
      .def_readwrite("occupied_byte", &PythonHookCache::occupied_byte)
      .def_readwrite("cache_size", &PythonHookCache::cache_size);

  /**
   * @brief Process a trace with a cache and return miss ratio.
   */
  m.def(
      "process_trace",
      [](cache_t& cache, reader_t& reader, int64_t start_req = 0,
         int64_t max_req = -1) {
        request_t* req = new_request();
        int64_t n_req = 0, n_hit = 0;
        bool hit;

        reset_reader(&reader);
        if (start_req > 0) {
          skip_n_req(&reader, start_req);
        }

        read_one_req(&reader, req);
        while (req->valid) {
          n_req += 1;
          hit = cache.get(&cache, req);
          if (hit) n_hit += 1;
          read_one_req(&reader, req);
          if (max_req > 0 && n_req >= max_req) {
            break;  // Stop if we reached the max request limit
          }
        }

        free_request(req);
        // return the miss ratio
        return n_req > 0 ? 1.0 - (double)n_hit / n_req : 0.0;
      },
      py::arg("cache"), py::arg("reader"), py::arg("start_req") = 0,
      py::arg("max_req") = -1,
      R"pbdoc(
            Process a trace with a cache and return miss ratio.

            This function processes trace data entirely on the C++ side to avoid
            data movement overhead between Python and C++.

            Args:
                cache (Cache): The cache instance to use for processing.
                reader (Reader): The trace reader instance.
                start_req (int): The starting request number to process from (default: 0, from the beginning).
                max_req (int): Maximum number of requests to process (-1 for no limit).

            Returns:
                float: Miss ratio (0.0 to 1.0).

            Example:
                >>> cache = libcachesim.LRU(1024*1024)
                >>> reader = libcachesim.open_trace("trace.csv", libcachesim.TraceType.CSV_TRACE)
                >>> miss_ratio = libcachesim.process_trace(cache, reader)
                >>> print(f"Miss ratio: {miss_ratio:.4f}")
      )pbdoc");

  /**
   * @brief Process a trace with a Python hook cache and return miss ratio.
   */
  m.def(
      "process_trace_python_hook",
      [](PythonHookCache& cache, reader_t& reader, int64_t start_req = 0,
         int64_t max_req = -1) {
        request_t* req = new_request();
        int n_req = 0, n_hit = 0;
        bool hit;

        reset_reader(&reader);
        if (start_req > 0) {
          skip_n_req(&reader, start_req);
        }

        read_one_req(&reader, req);
        while (req->valid) {
          n_req += 1;
          hit = cache.get(*req);
          if (hit) n_hit += 1;
          read_one_req(&reader, req);
          if (max_req > 0 && n_req >= max_req) {
            break;  // Stop if we reached the max request limit
          }
        }

        free_request(req);
        // return the miss ratio
        return n_req > 0 ? 1.0 - (double)n_hit / n_req : 0.0;
      },
      py::arg("cache"), py::arg("reader"), py::arg("start_req") = 0,
      py::arg("max_req") = -1,
      R"pbdoc(
            Process a trace with a Python hook cache and return miss ratio.

            This function processes trace data entirely on the C++ side to avoid
            data movement overhead between Python and C++. Specifically designed
            for PythonHookCache instances.

            Args:
                cache (PythonHookCache): The Python hook cache instance to use.
                reader (Reader): The trace reader instance.
                start_req (int): The starting request number to process from (0 for beginning).
                max_req (int): Maximum number of requests to process (-1 for no limit).

            Returns:
                float: Miss ratio (0.0 to 1.0).

            Example:
                >>> cache = libcachesim.PythonHookCachePolicy(1024*1024)
                >>> cache.set_hooks(init_hook, hit_hook, miss_hook, eviction_hook, remove_hook)
                >>> reader = libcachesim.open_trace("trace.csv", libcachesim.TraceType.CSV_TRACE)
                >>> miss_ratio = libcachesim.process_trace_python_hook(cache.cache, reader)
                >>> print(f"Miss ratio: {miss_ratio:.4f}")
      )pbdoc");

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}
