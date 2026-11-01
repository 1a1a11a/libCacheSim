// Parsing for llmCostModel.h; see there for the model strings.

#include "libCacheSim/llmCostModel.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Parse a whole decimal integer in [lo, hi], allowing surrounding spaces. */
static bool parse_int(const char *s, int64_t lo, int64_t hi, int64_t *out) {
  while (isspace((unsigned char)*s)) s++;
  if (*s == '\0') return false;
  char *end = NULL;
  errno = 0;
  const long long v = strtoll(s, &end, 10);
  if (errno != 0 || end == s) return false;
  while (isspace((unsigned char)*end)) end++;
  if (*end != '\0' || v < lo || v > hi) return false;
  *out = v;
  return true;
}

static bool parse_linear(const char *args, llm_cost_model_t *out, char *err,
                         size_t err_size) {
  const char *comma = strchr(args, ',');
  char a_str[64];
  if (comma == NULL || (size_t)(comma - args) >= sizeof(a_str)) {
    snprintf(err, err_size, "linear needs two coefficients, as linear:A,B");
    return false;
  }
  memcpy(a_str, args, comma - args);
  a_str[comma - args] = '\0';
  if (!parse_int(a_str, 1, INT32_MAX, &out->a) ||
      !parse_int(comma + 1, 0, INT32_MAX, &out->b)) {
    snprintf(err, err_size,
             "linear:%s: A must be an integer from 1 to %d and B one from 0 "
             "to %d, so that every block costs at least 1",
             args, INT32_MAX, INT32_MAX);
    return false;
  }
  return true;
}

static bool parse_table(const char *path, llm_cost_model_t *out, char *err,
                        size_t err_size) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    snprintf(err, err_size, "cannot open cost table '%s'", path);
    return false;
  }
  char *line = NULL;
  size_t cap = 0;
  int64_t n_alloc = 0, line_no = 0;
  bool ok = true;
  while (getline(&line, &cap, f) != -1) {
    line_no++;
    const char *p = line;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0' || *p == '#') continue;
    int64_t cost;
    if (!parse_int(p, 1, INT32_MAX, &cost)) {
      snprintf(err, err_size,
               "cost table '%s' line %lld: expected an integer from 1 to %d",
               path, (long long)line_no, INT32_MAX);
      ok = false;
      break;
    }
    if (out->n_table == n_alloc) {
      n_alloc = n_alloc == 0 ? 1024 : n_alloc * 2;
      out->table = realloc(out->table, n_alloc * sizeof(int64_t));
    }
    out->table[out->n_table++] = cost;
  }
  free(line);
  fclose(f);
  if (ok && out->n_table == 0) {
    snprintf(err, err_size, "cost table '%s' has no costs", path);
    ok = false;
  }
  if (!ok) llm_cost_model_free(out);
  return ok;
}

bool llm_cost_model_parse(const char *spec, llm_cost_model_t *out, char *err,
                          size_t err_size) {
  *out = llm_cost_model_uniform();
  if (spec == NULL || strcasecmp(spec, "uniform") == 0) return true;
  if (strcasecmp(spec, "position") == 0) {
    out->b = 1;
    return true;
  }
  if (strcasecmp(spec, "qwen3coder30b_blksz_16") == 0) {
    out->a = 865;
    out->b = 2;
    return true;
  }
  if (strncasecmp(spec, "linear:", 7) == 0) {
    if (parse_linear(spec + 7, out, err, err_size)) return true;
  } else if (strncasecmp(spec, "table:", 6) == 0) {
    if (parse_table(spec + 6, out, err, err_size)) return true;
  } else {
    snprintf(err, err_size, "unknown cost model '%s', expected %s", spec,
             LLM_COST_MODEL_CHOICES);
  }
  *out = llm_cost_model_uniform();
  return false;
}

void llm_cost_model_free(llm_cost_model_t *model) {
  free(model->table);
  model->table = NULL;
  model->n_table = 0;
}

int64_t llm_cost_model_max(const llm_cost_model_t *model, int64_t n_positions) {
  if (n_positions <= 0) return 0;
  if (model->n_table == 0) return llm_block_cost(model, n_positions - 1);
  int64_t max = 0;
  const int64_t n = n_positions < model->n_table ? n_positions : model->n_table;
  for (int64_t i = 0; i < n; i++) {
    if (model->table[i] > max) max = model->table[i];
  }
  return max;
}

bool llm_cost_model_is_uniform(const llm_cost_model_t *model) {
  if (model->n_table == 0) return model->b == 0;
  for (int64_t i = 1; i < model->n_table; i++) {
    if (model->table[i] != model->table[0]) return false;
  }
  return true;
}
