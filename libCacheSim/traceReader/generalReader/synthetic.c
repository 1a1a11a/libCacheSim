/*
 * Bounded Zipf sampling adapted from Apache Commons RNG's
 * RejectionInversionZipfSampler (Apache License 2.0):
 * https://commons.apache.org/proper/commons-rng/
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "synthetic.h"

#include <limits.h>
#include <math.h>

#include "syntheticRng.h"

/* Stable limits at alpha=1 for H(x)=(x^(1-alpha)-1)/(1-alpha)
 * and its inverse. These also avoid cancellation near alpha=1. */
static double expm1_over_x(double x) {
  if (fabs(x) > 1e-8) return expm1(x) / x;
  return 1 + x * 0.5 * (1 + x / 3 * (1 + x * 0.25));
}

static double log1p_over_x(double x) {
  if (fabs(x) > 1e-8) return log1p(x) / x;
  return 1 - x * (0.5 - x * (1.0 / 3 - x * 0.25));
}

static double h_integral(double x, double alpha) {
  double log_x = log(x);
  /* expm1(t) rounds to -1 below -40. Avoid overflowing t for large alpha. */
  if (1 - alpha < -40 / log_x) return -1 / (1 - alpha);
  return expm1_over_x((1 - alpha) * log_x) * log_x;
}

static double h_inverse(double x, double alpha) {
  /* Roundoff can put t just below -1 at the upper endpoint. */
  double t = fmax(-1, x * (1 - alpha));
  if (t == -1) return INFINITY;
  return exp(log1p_over_x(t) * x);
}

static bool parse_uint(const char *text, uint64_t max, uint64_t *value) {
  if (*text == '\0') return false;
  for (const char *p = text; *p != '\0'; p++) {
    if (!g_ascii_isdigit(*p)) return false;
  }
  errno = 0;
  *value = strtoull(text, NULL, 10);
  return errno != ERANGE && *value <= max;
}

int synthetic_setup(reader_t *reader) {
  synthetic_params_t params = {.seed = 1};
  uint64_t n_requests = 0;
  char **parts = g_strsplit(reader->trace_path, "_", -1);
  size_t count = g_strv_length(parts);
  const char *error = NULL;
  bool zipf = count > 0 && g_ascii_strcasecmp(parts[0], "Zipf") == 0;
  size_t n_fields = zipf ? 4 : 3;
  if ((!zipf && (count == 0 || g_ascii_strcasecmp(parts[0], "Uniform") != 0)) ||
      (count != n_fields && count != n_fields + 1)) {
    error = "expected Uniform_n_m[_seed] or Zipf_n_m_alpha[_seed]";
  } else if (!parse_uint(parts[1], INT64_MAX, &n_requests) || n_requests == 0) {
    error = "n must be an integer in [1, INT64_MAX]";
  } else if (!parse_uint(parts[2], INT32_MAX, &params.n_objects) ||
             params.n_objects == 0) {
    error = "m must be an integer in [1, INT32_MAX]";
  } else if (count == n_fields + 1 &&
             !parse_uint(parts[n_fields], UINT64_MAX, &params.seed)) {
    error = "seed must be an integer in [0, UINT64_MAX]";
  } else if (zipf) {
    char *end;
    errno = 0;
    params.alpha = g_ascii_strtod(parts[3], &end);
    if ((!g_ascii_isdigit(parts[3][0]) && parts[3][0] != '.') ||
        strspn(parts[3], "0123456789.eE+-") != strlen(parts[3]) ||
        *end != '\0' || (errno == ERANGE && params.alpha == 0) ||
        !isfinite(params.alpha) || params.alpha < 0) {
      error = "alpha must be a finite nonnegative number";
    }
  }
  g_strfreev(parts);

  const reader_init_param_t *init = &reader->init_params;
  if (error == NULL &&
      (init->trace_start_offset != 0 || init->binary_fmt_str != NULL ||
       init->block_size > 1 || init->time_field != 0 ||
       init->obj_id_field != 0 || init->obj_size_field != 0 ||
       init->obj_cost_field != 0 || init->op_field != 0 ||
       init->ttl_field != 0 || init->cnt_field != 0 ||
       init->tenant_field != 0 || init->next_access_vtime_field != 0 ||
       init->n_feature_fields != 0 || init->has_header ||
       (init->obj_id_is_num_set && !init->obj_id_is_num))) {
    error =
        "synthetic traces do not support file fields, headers, offsets, "
        "block splitting or string object IDs";
  }
  if (error != NULL) {
    fprintf(stderr, "error: invalid synthetic trace '%s': %s\n",
            reader->trace_path, error);
    return 1;
  }

  if (params.alpha > 0 && params.n_objects > 1) {
    params.h_integral_min = h_integral(1.5, params.alpha) - 1;
    params.h_integral_max = h_integral(params.n_objects + 0.5, params.alpha);
    params.squeeze = 2 - h_inverse(h_integral(2.5, params.alpha) -
                                       exp(-params.alpha * log(2)),
                                   params.alpha);
  }
  synthetic_params_t *state = malloc(sizeof(*state));
  if (state == NULL) {
    fprintf(stderr, "error: cannot allocate synthetic reader\n");
    return 1;
  }
  *state = params;
  reader->reader_params = state;
  reader->trace_format = SYNTHETIC_TRACE_FORMAT;
  reader->n_total_req = (int64_t)n_requests;
  reader->obj_id_is_num = true;
  return 0;
}

static uint64_t synthetic_object(const synthetic_params_t *params) {
  if (params->n_objects == 1) return 0;
  uint64_t draw = 0;
  if (params->alpha == 0) {
    /* Discard the incomplete residue group to avoid modulo bias. */
    uint64_t threshold = -params->n_objects % params->n_objects;
    uint64_t word;
    do {
      word = synthetic_random(params->position, draw++, params->seed);
    } while (word < threshold);
    return word % params->n_objects;
  }

  /* Hormann & Derflinger, rejection-inversion for monotone discrete
   * distributions (1996), with H shifted to support alpha <= 1 as well.
   * Rank k has probability proportional to k^-alpha; IDs are k-1. */
  for (;;) {
    uint64_t word = synthetic_random(params->position, draw++, params->seed);
    double u = params->h_integral_max +
               (double)(word >> 11) * 0x1.0p-53 *
                   (params->h_integral_min - params->h_integral_max);
    double x = h_inverse(u, params->alpha);
    /* Clamp before the integer conversion, including rounded endpoints. */
    double k = fmin((double)params->n_objects, fmax(1, floor(x + 0.5)));
    if (k - x <= params->squeeze || u >= h_integral(k + 0.5, params->alpha) -
                                             exp(-params->alpha * log(k))) {
      return (uint64_t)k - 1;
    }
  }
}

bool synthetic_has_next(const reader_t *reader) {
  const synthetic_params_t *params = reader->reader_params;
  return params->position < (uint64_t)reader->n_total_req &&
         (reader->cap_at_n_req <= 0 ||
          reader->n_read_req < reader->cap_at_n_req);
}

int synthetic_read_one_req(reader_t *reader, request_t *req) {
  synthetic_params_t *params = reader->reader_params;
  req->obj_id = synthetic_object(params);
  req->clock_time = 0;
  req->op = OP_NOP;
  req->next_access_vtime = -2;
  req->tenant_id = 0;
  req->ns = 0;
  req->n_features = 0;
  req->kv.key_size = 0;
  req->kv.val_size = 0;
  params->position++;
  return 0;
}
