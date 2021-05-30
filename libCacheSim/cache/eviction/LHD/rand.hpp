#pragma once

#include <stdint.h>

namespace misc {

  // placeholder random number generator from Knuth MMIX
  class Rand {
  public:
    Rand(uint64_t seed = 0) : state(seed) {}

    inline uint64_t next() {
      state = 6364136223846793005 * state + 1442695040888963407;
      return state;
    }

  private:
    uint64_t state;
  };

}
