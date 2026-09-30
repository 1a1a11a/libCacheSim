/**
 * Merlin: adaptive filter/staging/core FIFO queues with a ghost history.
 * Ported from TELOS-syslab/MerlinOSDI26AE, commit
 * 10907044ef8ed5360787b1e9a29aebf99c289826 (libCacheSim).
 */
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>

#include "dataStructure/hashtable/hashtable.h"
#include "dataStructure/minimalIncrementCBF.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/evictionAlgo.h"

namespace {
constexpr int kMaxFrequency = 7;
}  // namespace

/*
 * Merlin is implemented as a composite eviction policy built from four FIFO
 * queues:
 *
 *   filter  : first-touch admission area for newly seen objects.
 *   staging : probationary area for objects that look promising but are not
 *             hot enough to protect in the core cache yet.
 *   core    : protected area for objects that pass the adaptive hotness guard.
 *   ghost   : metadata-only history of recently evicted candidates.
 *
 * The AE counting Bloom filter (CBF) provides a compact history signal
 * across epochs. Its existing update semantics are retained.
 * The adaptive guard frequency is derived from frequency_population[] and
 * controls whether an object should be promoted into the protected core or stay
 * in the probationary path.
 */
namespace eviction {
struct merlin_params_t {
  request_t *req_local;
  cache_t *filter;
  cache_t *core;
  cache_t *staging;
  cache_t *ghost;
  // Byte budgets for the four internal queues.
  int64_t filter_limit;
  int64_t core_limit;
  int64_t staging_limit;
  int64_t ghost_limit;
  double filter_size_ratio;
  double staging_size_ratio;
  double ghost_size_ratio;
  // Compact history used to compare candidates and age past evidence.
  struct minimalIncrementCBF sketch;
  int32_t epoch_count;
  int32_t epoch_update;
  double sketch_scale;
  // Adaptive admission state.
  int32_t guard_freq;
  // Counts can exceed INT32_MAX on large traces; frequency indices stay small.
  // frequency_population[f] counts resident and ghost records with frequency
  // >= f. An object present in both staging and ghost contributes twice.
  std::array<int64_t, kMaxFrequency + 1> frequency_population{};
  int64_t evictions_in_epoch;  // Compare with the 64-bit resident object count.
  // Per-request state set by merlin_find() and consumed by merlin_insert().
  int32_t ghost_freq;
  bool ghost_hit;
  bool promote_ghost_to_core;
  request_t *req_staging;
};
}  // namespace eviction

extern "C" {

static const char *DEFAULT_CACHE_PARAMS =
    "filter-size-ratio=0.10,staging-size-ratio=0.05,ghost-size-ratio=1.00,"
    "epoch-update=32,sketch-scale=1.0";

static void merlin_free(cache_t *cache);
static bool merlin_get(cache_t *cache, const request_t *req);

static cache_obj_t *merlin_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *merlin_insert(cache_t *cache, const request_t *req);
static cache_obj_t *merlin_to_evict(cache_t *cache, const request_t *req);
static void merlin_evict(cache_t *cache, const request_t *req);
static bool merlin_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t merlin_get_occupied_byte(const cache_t *cache);
static inline int64_t merlin_get_n_obj(const cache_t *cache);
static inline bool merlin_can_insert(cache_t *cache, const request_t *req);
static cache_obj_t *add_to_ghost(cache_t *cache, const request_t *req,
                                 int freq);
static void merlin_parse_params(eviction::merlin_params_t *params,
                                const char *cache_specific_params);

cache_t *merlin_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  // Parse before allocating caches or sketches: -e print needs no memory.
  eviction::merlin_params_t config{};
  merlin_parse_params(&config, DEFAULT_CACHE_PARAMS);
  merlin_parse_params(&config, cache_specific_params);
  if (ccache_params.cache_size > INT64_MAX) {
    fprintf(stderr, "merlin parameter error: cache size exceeds INT64_MAX\n");
    exit(EXIT_FAILURE);
  }
  cache_t *cache =
      cache_struct_init("merlin", ccache_params, cache_specific_params);
  cache->eviction_params = new eviction::merlin_params_t(config);

  cache->cache_init = merlin_init;
  cache->cache_free = merlin_free;
  cache->get = merlin_get;
  cache->find = merlin_find;
  cache->insert = merlin_insert;
  cache->evict = merlin_evict;
  cache->to_evict = merlin_to_evict;
  cache->remove = merlin_remove;
  cache->get_n_obj = merlin_get_n_obj;
  cache->get_occupied_byte = merlin_get_occupied_byte;
  cache->can_insert = merlin_can_insert;

  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);

  params->req_local = new_request();
  params->req_staging = new_request();

  // Split the user-visible cache budget among Merlin's internal queues.
  // The ghost queue stores history and can be sized independently.
  const int64_t capacity = ccache_params.cache_size;
  params->filter_limit = std::min(
      capacity,
      std::max<int64_t>(
          1, static_cast<long double>(params->filter_size_ratio) * capacity));
  params->staging_limit = std::min(
      capacity - params->filter_limit,
      std::max<int64_t>(
          1, static_cast<long double>(params->staging_size_ratio) * capacity));
  params->core_limit = capacity - params->filter_limit - params->staging_limit;
  const long double ghost_size =
      static_cast<long double>(capacity) * params->ghost_size_ratio;
  // 2^63 is exact even on platforms where long double has double precision.
  if (ghost_size >= 0x1p63L) {
    fprintf(stderr, "merlin parameter error: ghost size overflows int64\n");
    exit(EXIT_FAILURE);
  }
  params->ghost_limit = static_cast<int64_t>(ghost_size);
  common_cache_params_t ccache_params_filter = ccache_params;
  ccache_params_filter.cache_size = params->filter_limit;
  common_cache_params_t ccache_params_staging = ccache_params;
  ccache_params_staging.cache_size = params->staging_limit;
  common_cache_params_t ccache_params_core = ccache_params;
  ccache_params_core.cache_size = params->core_limit;
  common_cache_params_t ccache_params_ghost = ccache_params;
  ccache_params_ghost.cache_size = params->ghost_limit;
  params->filter = FIFO_init(ccache_params_filter, nullptr);
  params->staging = FIFO_init(ccache_params_staging, nullptr);
  params->core = FIFO_init(ccache_params_core, nullptr);
  params->ghost = FIFO_init(ccache_params_ghost, nullptr);
  cache->obj_md_size =
      ccache_params.consider_obj_metadata ? sizeof(MERLIN_obj_metadata_t) : 0;
  params->filter->obj_md_size = cache->obj_md_size;
  params->staging->obj_md_size = cache->obj_md_size;
  params->core->obj_md_size = cache->obj_md_size;
  // Ghost capacity is measured in represented object bytes, as in AE.
  params->ghost->obj_md_size = 0;
  params->epoch_count = 0;
  params->evictions_in_epoch = 0;
  params->guard_freq = 2;

  // Keep the AE entry cap. This can still allocate ~8 GiB of counters;
  // reduce sketch-scale to lower the memory cost.
  int cbf_size =
      std::max(1.0, std::min(ccache_params.cache_size * params->sketch_scale,
                             double(1 << 30)));
  int ret = minimalIncrementCBF_init(&params->sketch, cbf_size, 0.001);
  if (ret != 0) {
    ERROR("CBF init failed\n");
  }
  params->ghost_hit = false;
  params->promote_ghost_to_core = false;

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN,
           "merlin-%.2lf-%.2lf-%.2lf-%d-%.2f", params->filter_size_ratio,
           params->staging_size_ratio, params->ghost_size_ratio,
           params->epoch_update, params->sketch_scale);
  return cache;
}

static void merlin_free(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  params->filter->cache_free(params->filter);
  params->staging->cache_free(params->staging);
  params->core->cache_free(params->core);
  params->ghost->cache_free(params->ghost);
  minimalIncrementCBF_free(&params->sketch);
  free_request(params->req_local);
  free_request(params->req_staging);
  delete params;
  cache_struct_free(cache);
}

static bool merlin_get(cache_t *cache, const request_t *req) {
  if (cache->prefetcher != nullptr) {
    fprintf(stderr,
            "merlin configuration error: prefetching is not supported\n");
    exit(EXIT_FAILURE);
  }
  return cache_get_base(cache, req);
}

static cache_obj_t *merlin_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  if (update_cache) {
    params->ghost_hit = false;
    params->promote_ghost_to_core = false;
    params->ghost_freq = 0;
  }
  cache_t *queues[] = {params->filter, params->staging, params->core};
  for (cache_t *queue : queues) {
    cache_obj_t *resident = hashtable_find(queue->hashtable, req);
    if (resident == nullptr) continue;
#ifdef SUPPORT_TTL
    if (resident->exp_time != 0 && resident->exp_time < req->clock_time) {
      if (update_cache) merlin_remove(cache, resident->obj_id);
      return nullptr;
    }
#endif
    if (update_cache) {
      resident->next_access_vtime = req->next_access_vtime;
      if (resident->freq < INT32_MAX) ++resident->freq;
      if (resident->MERLIN.freq < kMaxFrequency) {
        ++resident->MERLIN.freq;
        ++params->frequency_population[resident->MERLIN.freq];
      }
    }
    return resident;
  }
  if (!update_cache) return nullptr;
  cache_obj_t *obj;
  // Ghost hits are misses from the user's point of view, but they carry
  // useful history. merlin_insert() consumes this state to decide whether
  // the returning object enters staging or core.
  obj = params->ghost->find(params->ghost, req, false);
  if (obj != nullptr) {
    if (obj->MERLIN.freq < kMaxFrequency) {
      obj->MERLIN.freq++;
      params->frequency_population[obj->MERLIN.freq] += 1;
    }
    params->ghost_hit = true;
    params->ghost_freq = obj->MERLIN.freq;
    if (obj->MERLIN.freq >= params->guard_freq) {
      params->promote_ghost_to_core = true;
    }
  }
  return nullptr;
}

static void decrease_population(
    std::array<int64_t, kMaxFrequency + 1> &frequency_population,
    int32_t ori_freq, int32_t dest_freq) {
  // frequency_population is cumulative: moving an object from ori_freq to
  // dest_freq removes it from every threshold it no longer satisfies.
  for (int i = ori_freq; i > dest_freq; i--) {
    DEBUG_ASSERT(frequency_population[i] > 0);
    frequency_population[i]--;
  }
}

static cache_obj_t *merlin_insert(cache_t *cache, const request_t *req) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  cache_obj_t *obj = nullptr;
  if (!params->ghost_hit && params->staging->get_n_obj(params->staging) == 0 &&
      params->filter->get_n_obj(params->filter) == 0) {
    // Warmup: before the admission queues contain enough signal, admit
    // directly to core so the cache can start serving hits immediately.
    obj = params->core->insert(params->core, req);
    obj->MERLIN.freq = 0;
    params->frequency_population[0]++;
  } else if (params->ghost_hit) {
    params->ghost_hit = false;
    if (params->promote_ghost_to_core) {
      // A returning ghost object whose historical frequency has
      // crossed guard_freq is treated as hot and protected.
      params->promote_ghost_to_core = false;
      obj = params->core->insert(params->core, req);
      obj->MERLIN.freq = params->ghost_freq;
      if (!params->ghost->remove(params->ghost, obj->obj_id)) {
        // Eviction may have trimmed this ghost after find().
        for (int f = 0; f <= params->ghost_freq; ++f)
          ++params->frequency_population[f];
      }
    } else {
      // A ghost hit below the guard threshold is promising but
      // still probationary, so it enters staging and is tracked
      // in the sketch.
      minimalIncrementCBF_add(&params->sketch, &req->obj_id, sizeof(obj_id_t));
      obj = params->staging->insert(params->staging, req);
      obj->MERLIN.freq = 0;
      // Staging and ghost deliberately overlap on this path.
      obj->MERLIN.inghost = 1;
      params->frequency_population[0]++;
    }
  } else {
    // First-seen objects enter the filter. They must earn further
    // protection through hits or sketch evidence.
    obj = params->filter->insert(params->filter, req);
    obj->MERLIN.freq = 0;
    params->frequency_population[0]++;
  }
  return obj;
}

static cache_obj_t *merlin_to_evict(cache_t *cache, const request_t *req) {
  // Like S3FIFO, choosing a victim can require multiple queue movements.
  DEBUG_ASSERT(false);
  return nullptr;
}

static cache_obj_t *add_to_ghost(cache_t *cache, const request_t *req,
                                 int freq) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  cache_obj_t *object = params->ghost->insert(params->ghost, req);
  object->MERLIN.freq = freq;
#ifdef SUPPORT_TTL
  object->exp_time = 0;
#endif
  return object;
}

static void remove_from_ghost(cache_t *cache, const request_t *req) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  cache_obj_t *obj = params->ghost->find(params->ghost, req, false);
  if (obj != nullptr) {
    int ori_freq = obj->MERLIN.freq;
    decrease_population(params->frequency_population, ori_freq, -1);
    params->ghost->remove(params->ghost, obj->obj_id);
  }
}

static void adjust_core(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  // If core exceeds its budget, demote its FIFO victims to staging. This
  // preserves their metadata while making room for newly promoted items.
  while (params->core_limit < params->core->get_occupied_byte(params->core)) {
    cache_obj_t *obj_to_evict = params->core->to_evict(params->core, nullptr);
    int ori_freq = obj_to_evict->MERLIN.freq;
    assert(ori_freq >= 0 && ori_freq <= kMaxFrequency);
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    cache_obj_t *new_obj =
        params->staging->insert(params->staging, params->req_local);
    new_obj->MERLIN.freq = ori_freq;
#ifdef SUPPORT_TTL
    new_obj->exp_time = obj_to_evict->exp_time;
#endif
    params->core->remove(params->core, obj_to_evict->obj_id);
  }
}

static void evict_staging(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  // Staging eviction drops probationary objects and removes their
  // contribution from the cumulative hotness distribution.
  cache_obj_t *staging_to_evict =
      params->staging->to_evict(params->staging, nullptr);
  int ori_freq = staging_to_evict->MERLIN.freq;
  decrease_population(params->frequency_population, ori_freq, -1);
  params->staging->remove(params->staging, staging_to_evict->obj_id);
}

static bool promote_filter_candidate(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  cache_obj_t *filter_to_evict =
      params->filter->to_evict(params->filter, nullptr);
  cache_obj_t *staging_to_evict =
      params->staging->to_evict(params->staging, nullptr);
  if (filter_to_evict == nullptr || staging_to_evict == nullptr) {
    return false;
  }
  if (staging_to_evict->MERLIN.freq > 0) {
    return false;
  }
  // Compare the oldest filter candidate with the oldest cold staging
  // candidate using sketch estimates. A higher sketch value means the
  // filter object has stronger recent history and can replace staging.
  int filter_value =
      minimalIncrementCBF_estimate(&params->sketch, &filter_to_evict->obj_id,
                                   sizeof(filter_to_evict->obj_id));
  if (filter_value > params->epoch_update) {  // false positive
    return false;
  }
  int staging_value =
      minimalIncrementCBF_estimate(&params->sketch, &staging_to_evict->obj_id,
                                   sizeof(staging_to_evict->obj_id));
  if (filter_value > staging_value) {
    // The filter candidate wins: move it into staging and remember it
    // in ghost so a future return can reuse this frequency state.
    minimalIncrementCBF_add(&params->sketch, &filter_to_evict->obj_id,
                            sizeof(obj_id_t));
    copy_cache_obj_to_request(params->req_staging, filter_to_evict);
    cache_obj_t *new_obj =
        params->staging->insert(params->staging, params->req_staging);
    new_obj->MERLIN.freq = 0;
#ifdef SUPPORT_TTL
    new_obj->exp_time = filter_to_evict->exp_time;
#endif
    new_obj->MERLIN.inghost = 1;
    params->frequency_population[new_obj->MERLIN.freq]++;
    add_to_ghost(cache, params->req_staging, filter_to_evict->MERLIN.freq);
    params->filter->remove(params->filter, filter_to_evict->obj_id);
    return true;
  }
  return false;
}

static int adjust_staging(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  while (params->staging->get_occupied_byte(params->staging) > 0) {
    cache_obj_t *obj_to_evict =
        params->staging->to_evict(params->staging, nullptr);
    if (obj_to_evict->MERLIN.freq == 0) {
      // Stop at the first cold staging object. The caller can evict
      // it or compare it against a filter candidate.
      return 1;
    }
    // Hot staging objects graduate to core. Frequency is decremented by
    // one to charge a promotion cost and avoid over-protecting bursts.
    int ori_freq = obj_to_evict->MERLIN.freq;
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    if (obj_to_evict->MERLIN.inghost) {
      remove_from_ghost(cache, params->req_local);
      obj_to_evict->MERLIN.inghost = 0;
    }
    minimalIncrementCBF_add(&params->sketch, &params->req_local->obj_id,
                            sizeof(obj_id_t));
    cache_obj_t *new_obj =
        params->core->insert(params->core, params->req_local);
    new_obj->MERLIN.freq = ori_freq - 1;
#ifdef SUPPORT_TTL
    new_obj->exp_time = obj_to_evict->exp_time;
#endif
    decrease_population(params->frequency_population, ori_freq, ori_freq - 1);
    params->staging->remove(params->staging, obj_to_evict->obj_id);
  }
  return 0;
}

static int adjust_filter(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  int has_evicted = 0;

  // Make room in filter. Hot filter victims bypass staging and enter
  // core; cold victims are either compared against staging or recorded in
  // ghost before being removed.
  while (!has_evicted &&
         params->filter->get_occupied_byte(params->filter) > 0) {
    cache_obj_t *obj_to_evict =
        params->filter->to_evict(params->filter, nullptr);
    int ori_freq = obj_to_evict->MERLIN.freq;
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    if (ori_freq >= params->guard_freq) {
      cache_obj_t *new_obj =
          params->core->insert(params->core, params->req_local);
      new_obj->MERLIN.freq = 0;
#ifdef SUPPORT_TTL
      new_obj->exp_time = obj_to_evict->exp_time;
#endif
      decrease_population(params->frequency_population, ori_freq, 0);
    } else {
      has_evicted = 1;
      if (promote_filter_candidate(cache)) {
        // promote_filter_candidate() has already moved the filter candidate;
        // evict the losing staging candidate to complete the swap.
        evict_staging(cache);
        return has_evicted;
      } else {
        // The filter candidate loses or cannot be compared; keep
        // only its metadata in ghost.
        add_to_ghost(cache, params->req_local, ori_freq);
      }
    }
    minimalIncrementCBF_add(&params->sketch, &params->req_local->obj_id,
                            sizeof(obj_id_t));
    params->filter->remove(params->filter, obj_to_evict->obj_id);
  }
  return has_evicted;
}

static void adjust_ghost(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  // Bound ghost memory and remove stale metadata from frequency_population.
  while (params->ghost->get_occupied_byte(params->ghost) >
         params->ghost_limit) {
    cache_obj_t *obj_to_evict = params->ghost->to_evict(params->ghost, nullptr);
    int ori_freq = obj_to_evict->MERLIN.freq;
    params->ghost->remove(params->ghost, obj_to_evict->obj_id);
    decrease_population(params->frequency_population, ori_freq, -1);
  }
}

static void adjust_guard(cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  // Choose the smallest frequency threshold whose population roughly
  // matches the protected core population. This adapts the promotion
  // boundary as the workload gets hotter or colder. Saturate at kMaxFrequency:
  // frequency-7 objects remain eligible for guard-based promotion.
  int guard_freq = params->guard_freq;
  int64_t core_population = params->core->get_n_obj(params->core);
  if (params->frequency_population[guard_freq] > core_population) {
    while (guard_freq < kMaxFrequency &&
           params->frequency_population[guard_freq] > core_population) {
      guard_freq++;
    }
  } else {
    while (guard_freq > 1 &&
           params->frequency_population[guard_freq - 1] < core_population) {
      guard_freq--;
    }
  }
  params->guard_freq = guard_freq;
}

static void merlin_evict(cache_t *cache, const request_t *req) {
  if (cache == nullptr || cache->eviction_params == nullptr) {
    fprintf(stderr, "Error: cache or cache->eviction_params is null\n");
    return;
  }

  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  params->evictions_in_epoch += 1;
  if (params->evictions_in_epoch >=
      std::max<int64_t>(1, cache->get_n_obj(cache))) {
    // Resident count can shrink between evictions, so equality may be skipped.
    // Treat one full-cache worth of evictions as an epoch. Periodic
    // sketch decay prevents old popularity from dominating forever.
    params->evictions_in_epoch = 0;
    params->epoch_count += 1;
    if (params->epoch_count >= params->epoch_update) {
      params->epoch_count = 0;
      minimalIncrementCBF_decay(&params->sketch);
    }
  }

  adjust_guard(cache);

  if (cache->get_n_obj(cache) == 0) return;
  if (params->filter->get_n_obj(params->filter) > 0 &&
      (req == nullptr || params->filter->get_occupied_byte(params->filter) +
                                 req->obj_size + cache->obj_md_size >
                             params->filter_limit)) {
    // New insert would overflow filter, so free space from the
    // first-touch admission path.
    adjust_filter(cache);
  } else {
    // Otherwise free space from staging. Hot staging objects may be
    // promoted first, which can in turn require core demotion.
    int ret = 0;
    while (ret == 0) {
      adjust_core(cache);
      ret = adjust_staging(cache);
      if (!ret &&
          params->core->get_occupied_byte(params->core) <= params->core_limit) {
        // Explicit eviction/removal can leave an underfull core
        // and no staging candidate. Do not spin waiting for one.
        cache_obj_t *victim = params->core->to_evict(params->core, nullptr);
        if (victim != nullptr) {
          decrease_population(params->frequency_population, victim->MERLIN.freq,
                              -1);
          params->core->remove(params->core, victim->obj_id);
        } else {
          adjust_filter(cache);
        }
        adjust_ghost(cache);
        return;
      }
    }
    // Give an old filter candidate one chance to replace the cold
    // staging victim before evicting staging.
    promote_filter_candidate(cache);
    evict_staging(cache);
  }
  adjust_ghost(cache);
}

static bool merlin_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  bool removed = false;
  cache_t *queues[] = {params->filter, params->staging, params->core,
                       params->ghost};
  for (cache_t *queue : queues) {
    cache_obj_t *obj = hashtable_find_obj_id(queue->hashtable, obj_id);
    if (obj != nullptr) {
      decrease_population(params->frequency_population, obj->MERLIN.freq, -1);
      queue->remove(queue, obj_id);
      removed = true;
    }
  }
  params->ghost_hit = false;
  params->promote_ghost_to_core = false;
  return removed;
}

static inline int64_t merlin_get_occupied_byte(const cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  return params->filter->get_occupied_byte(params->filter) +
         params->staging->get_occupied_byte(params->staging) +
         params->core->get_occupied_byte(params->core);
}

static inline int64_t merlin_get_n_obj(const cache_t *cache) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  return params->filter->get_n_obj(params->filter) +
         params->staging->get_n_obj(params->staging) +
         params->core->get_n_obj(params->core);
}

static inline bool merlin_can_insert(cache_t *cache, const request_t *req) {
  auto *params =
      static_cast<eviction::merlin_params_t *>(cache->eviction_params);
  return req->obj_size > 0 && req->obj_size <= params->filter_limit &&
         cache->obj_md_size <= params->filter_limit - req->obj_size &&
         cache_can_insert_default(cache, req);
}

static void merlin_param_error(const char *message) {
  fprintf(stderr, "merlin parameter error: %s\n", message);
  exit(EXIT_FAILURE);
}

static void merlin_parse_params(eviction::merlin_params_t *params,
                                const char *cache_specific_params) {
  if (cache_specific_params == nullptr || *cache_specific_params == '\0')
    return;
  char *storage = strdup(cache_specific_params);
  char *cursor = storage;
  char *token;
  bool print = false;
  while ((token = strsep(&cursor, ",")) != nullptr) {
    while (isspace(static_cast<unsigned char>(*token))) ++token;
    char *tail = token + strlen(token);
    while (tail > token && isspace(static_cast<unsigned char>(tail[-1])))
      *--tail = '\0';
    if (strcasecmp(token, "print") == 0) {
      print = true;
      continue;
    }
    char *value = strchr(token, '=');
    if (value == nullptr) merlin_param_error("expected key=value");
    *value++ = '\0';
    if (*value == '\0') merlin_param_error("empty value");
    char *end = nullptr;
    errno = 0;
    const double number = strtod(value, &end);
    if (end == value || *end != '\0' || errno == ERANGE ||
        !std::isfinite(number))
      merlin_param_error("expected a finite numeric value");
    if (strcasecmp(token, "filter-size-ratio") == 0)
      params->filter_size_ratio = number;
    else if (strcasecmp(token, "staging-size-ratio") == 0)
      params->staging_size_ratio = number;
    else if (strcasecmp(token, "ghost-size-ratio") == 0)
      params->ghost_size_ratio = number;
    else if (strcasecmp(token, "sketch-scale") == 0)
      params->sketch_scale = number;
    else if (strcasecmp(token, "epoch-update") == 0) {
      if (number < 1 || number > INT32_MAX || std::floor(number) != number)
        merlin_param_error("epoch-update must be an integer in [1, INT32_MAX]");
      params->epoch_update = static_cast<int32_t>(number);
    } else
      merlin_param_error("unknown parameter");
  }
  free(storage);
  if (params->filter_size_ratio <= 0 || params->filter_size_ratio >= 1 ||
      params->staging_size_ratio <= 0 || params->staging_size_ratio >= 1 ||
      params->filter_size_ratio + params->staging_size_ratio >= 1)
    merlin_param_error(
        "positive filter/staging ratios must sum to less than 1");
  if (params->ghost_size_ratio < 0 || params->sketch_scale <= 0)
    merlin_param_error(
        "ghost-size-ratio must be nonnegative; sketch-scale must be positive");
  if (print) {
    printf(
        "merlin current parameters: "
        "filter-size-ratio=%.6g,staging-size-ratio=%.6g,"
        "ghost-size-ratio=%.6g,epoch-update=%d,sketch-scale=%.6g\n",
        params->filter_size_ratio, params->staging_size_ratio,
        params->ghost_size_ratio, params->epoch_update, params->sketch_scale);
    exit(EXIT_SUCCESS);
  }
}

}  // extern "C"
