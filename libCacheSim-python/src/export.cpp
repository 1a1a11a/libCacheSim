// libcachesim_python - libCacheSim Python bindings
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include "export.h"

#include "exception.h"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace libcachesim {

PYBIND11_MODULE(libcachesim_python, m) {
  m.doc() = "libcachesim_python";

  // NOTE(haocheng): can use decentralized interface holder to export all the
  // methods if the codebase is large enough

  export_cache(m);
  export_reader(m);
  export_analyzer(m);
  export_misc(m);

  // NOTE(haocheng): register exception to make it available in Python
  register_exception(m);

#ifdef VERSION_INFO
  m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}

}  // namespace libcachesim
