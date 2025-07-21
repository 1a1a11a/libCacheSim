// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include "exception.h"

#include <pybind11/pybind11.h>

namespace libcachesim {

namespace py = pybind11;

void register_exception(py::module& m) {
  static py::exception<CacheException> exc_cache(m, "CacheException");
  static py::exception<ReaderException> exc_reader(m, "ReaderException");

  py::register_exception_translator([](std::exception_ptr p) {
    try {
      if (p) std::rethrow_exception(p);
    } catch (const CacheException& e) {
      py::set_error(exc_cache, e.what());
    } catch (const ReaderException& e) {
      py::set_error(exc_reader, e.what());
    }
  });

  py::register_exception_translator([](std::exception_ptr p) {
    try {
      if (p) std::rethrow_exception(p);
    } catch (const std::bad_alloc& e) {
      PyErr_SetString(PyExc_MemoryError, e.what());
    } catch (const std::invalid_argument& e) {
      PyErr_SetString(PyExc_ValueError, e.what());
    } catch (const std::out_of_range& e) {
      PyErr_SetString(PyExc_IndexError, e.what());
    } catch (const std::domain_error& e) {
      PyErr_SetString(PyExc_ValueError,
                      ("Domain error: " + std::string(e.what())).c_str());
    } catch (const std::overflow_error& e) {
      PyErr_SetString(PyExc_OverflowError, e.what());
    } catch (const std::range_error& e) {
      PyErr_SetString(PyExc_ValueError,
                      ("Range error: " + std::string(e.what())).c_str());
    } catch (const std::runtime_error& e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
    } catch (const std::exception& e) {
      PyErr_SetString(PyExc_RuntimeError,
                      ("C++ exception: " + std::string(e.what())).c_str());
    }
  });
}

}  // namespace libcachesim
