#pragma once

#include <sstream>
#include <string>

namespace misc {

inline std::string bytes(uint64_t size) {
  std::stringstream ss;
  const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};

  uint32_t i;
  for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
    if (size < 1024 * 1024) {
      break;
    } else {
      size /= 1024;
    }
  }

  if (i == sizeof(suffixes) / sizeof(suffixes[0])) {
    --i;
  }

  ss << size << suffixes[i];
  return ss.str();
}

}  // namespace misc
