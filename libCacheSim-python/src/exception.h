// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#pragma once

#include <pybind11/pybind11.h>

#include <stdexcept>
#include <string>

namespace libcachesim {

namespace py = pybind11;

class CacheException : public std::runtime_error {
 public:
  explicit CacheException(const std::string& message)
      : std::runtime_error("CacheException: " + message) {}
};

class ReaderException : public std::runtime_error {
 public:
  explicit ReaderException(const std::string& message)
      : std::runtime_error("ReaderException: " + message) {}
};

void register_exception(py::module& m);

}  // namespace libcachesim
