/*
 * Philox2x64-10, adapted from Random123's philox.h to return one word.
 * https://github.com/DEShawResearch/random123
 *
 * Copyright 2010-2011, D. E. Shaw Research.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions, and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions, and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of D. E. Shaw Research nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <stdint.h>

/* Each source position has its own draw counter. Rejection sampling never
 * changes subsequent requests, and seeking needs no replay or shared RNG. */
static inline uint64_t synthetic_random(uint64_t position, uint64_t draw,
                                        uint64_t seed) {
  for (int round = 0; round < 10; round++) {
    __uint128_t product = (__uint128_t)position * UINT64_C(0xd2b74407b1ce6e93);
    position = (uint64_t)(product >> 64) ^ seed ^ draw;
    draw = (uint64_t)product;
    seed += UINT64_C(0x9e3779b97f4a7c15);
  }
  return position;
}
