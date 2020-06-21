//
//  heatmap_thread.c
//  libCacheSim
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "libCacheSim/heatmap.h"


#ifdef __cplusplus
extern "C"
{
#endif


/**
 * thread function for computing LRU heatmap of type start_time_end_time
 *
 * @param data contains order+1
 * @param user_data passed in param
 */
void hm_LRU_hr_st_et_thread(gpointer data, gpointer user_data) {

  guint64 i, j, hit_count_all, miss_count_all, hit_count_interval, miss_count_interval;
  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  reader_t *reader_thread = clone_reader(params->reader);
  GArray *break_points = params->break_points;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;
  guint64 cache_size = (guint64) params->cache->size;
  // distance to last access
  gint64 *last_access = params->last_access_dist;
  gint64 *stack_dist = params->stack_dist;

  size_t order = GPOINTER_TO_SIZE(data) - 1;
  guint64 real_start = g_array_index(break_points, guint64, order);
  gboolean interval_hit_ratio_b = params->interval_hit_ratio_b;
  double decay_coefficient_lf = params->ewma_coefficient_lf;

  hit_count_all = 0;
  miss_count_all = 0;


  skip_N_elements(reader_thread, g_array_index(break_points, guint64, order));

  for (i = order; i < break_points->len - 1; i++) {
    hit_count_interval = 0;
    miss_count_interval = 0;
    for (j = g_array_index(break_points, guint64, i); j < g_array_index(break_points, guint64, i + 1); j++) {
      if (stack_dist[j] == -1) {
        miss_count_interval++;
      } else if (last_access[j] - (long long) (j - real_start) <= 0 && stack_dist[j] < (long long) cache_size)
        hit_count_interval++;
      else
        miss_count_interval++;
    }
    hit_count_all += hit_count_interval;
    miss_count_all += miss_count_interval;
    if (interval_hit_ratio_b) {
      if (i == order)
        // no decay for first pixel
        dd->matrix[order][i] = (double) (hit_count_all) / (hit_count_all + miss_count_all);
      else {
        dd->matrix[order][i] = dd->matrix[order][i - 1] * decay_coefficient_lf +
                               (1 - decay_coefficient_lf) * (double) (hit_count_interval) /
                               (hit_count_interval + miss_count_interval);
      }
    } else
      dd->matrix[order][i] = (double) (hit_count_all) / (hit_count_all + miss_count_all);
  }

  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
  close_cloned_reader(reader_thread);
}


/**
 * thread function for computing LRU heatmap of type interval_cacheSize
 *
 * @param data contains order+1
 * @param user_data passed in param
 */
void hm_LRU_hr_interval_size_thread(gpointer data, gpointer user_data) {

  guint64 i, j, hit_count_interval, miss_count_interval;

  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  reader_t *reader_thread = clone_reader(params->reader);
  GArray *break_points = params->break_points;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;


  size_t order = GPOINTER_TO_SIZE(data) - 1;
  // plus 1 because we don't need cache size 0
  gint64 cache_size = (gint64) params->bin_size * order;
  if (cache_size == 0) {
    for (i = 0; i < break_points->len - 1; i++)
      dd->matrix[i][order] = 0;
  } else {
    gint64 *stack_dist = params->stack_dist;
    double ewma_coefficient_lf = params->ewma_coefficient_lf;


    for (i = 0; i < break_points->len - 1; i++) {
      hit_count_interval = 0;
      miss_count_interval = 0;
      for (j = g_array_index(break_points, guint64, i); j < g_array_index(break_points, guint64, i + 1); j++) {
        if (stack_dist[j] == -1) {
          miss_count_interval++;
        } else if (stack_dist[j] < (long long) cache_size)
          hit_count_interval++;
        else
          miss_count_interval++;
      }

      if (i == 0)
        // no decay for first pixel
        dd->matrix[i][order] = (double) (hit_count_interval) / (hit_count_interval + miss_count_interval);
      else {
        dd->matrix[i][order] = dd->matrix[i - 1][order] * ewma_coefficient_lf +
                               (1 - ewma_coefficient_lf) * (double) (hit_count_interval) /
                               (hit_count_interval + miss_count_interval);
      }
    }
  }

  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
  close_cloned_reader(reader_thread);
}


/**
 * thread function for computing nonLRU heatmap of type start_time_end_time
 *
 * @param data contains order+1
 * @param user_data passed in param
 */
void hm_LRU_hr_st_size_thread(gpointer data, gpointer user_data) {

  guint64 i, j, hit_count, miss_count;
  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  reader_t *reader_thread = clone_reader(params->reader);
  GArray *break_points = params->break_points;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;
  guint64 cache_size = (guint64) params->cache->size;
  gint64 *last_access = params->last_access_dist;
  gint64 *stack_dist = params->stack_dist;

  size_t order = GPOINTER_TO_SIZE(data) - 1;
  guint64 real_start = g_array_index(break_points, guint64, order);

  hit_count = 0;
  miss_count = 0;


  // unnecessary ?
  skip_N_elements(reader_thread, g_array_index(break_points, guint64, order));

  for (i = order; i < break_points->len - 1; i++) {

    for (j = g_array_index(break_points, guint64, i); j < g_array_index(break_points, guint64, i + 1); j++) {
      if (stack_dist[j] == -1)
        miss_count++;
      else if (last_access[j] - (long long) (j - real_start) <= 0 && stack_dist[j] < (long long) cache_size)
        hit_count++;
      else
        miss_count++;
    }
    dd->matrix[order][i] = (double) (hit_count) / (hit_count + miss_count);
  }

  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
  close_cloned_reader(reader_thread);
}


void hm_rd_distribution_thread(gpointer data, gpointer user_data) {

  guint64 j;
  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  GArray *break_points = params->break_points;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;
  gint64 *stack_dist = params->stack_dist;
  double log_base = dd->log_base;

  size_t order = GPOINTER_TO_SIZE(data) - 1;
  double *array = dd->matrix[order];

  if (order != break_points->len - 1) {
    for (j = g_array_index(break_points, guint64, order); j < g_array_index(break_points, guint64, order + 1); j++) {
      if (stack_dist[j] == 0 || stack_dist[j] == 1)
        array[0] += 1;
      else
        array[(long) (log(stack_dist[j]) / (log(log_base)))] += 1;
    }
  }

  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
}

void hm_rd_distribution_CDF_thread(gpointer data, gpointer user_data) {

  guint64 j;
  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  GArray *break_points = params->break_points;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;
  gint64 *stack_dist = params->stack_dist;
  double log_base = dd->log_base;

  size_t order = GPOINTER_TO_SIZE(data) - 1;
  double *array = dd->matrix[order];


  if (order != break_points->len - 1) {
    for (j = g_array_index(break_points, guint64, order); j < g_array_index(break_points, guint64, order + 1); j++) {
      if (stack_dist[j] == 0 || stack_dist[j] == 1)
        array[0] += 1;
      else
        array[(long) (log(stack_dist[j]) / (log(log_base)))] += 1;
    }
  }

  for (j = 1; j < dd->ylength; j++)
    array[j] += array[j - 1];

  for (j = 0; j < dd->ylength; j++)
    array[j] = array[j] / array[dd->ylength - 1];

  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
}


/**
 * thread function for computing LRU effective size heatmap of type start_time_end_time
 *
 * @param data contains order+1
 * @param user_data passed in param
 */
void hm_LRU_effective_size_thread(gpointer data, gpointer user_data) {
  guint64 i, j;
  mt_params_hm_t *params = (mt_params_hm_t *) user_data;
  reader_t *reader_thread = clone_reader(params->reader);
  GArray *break_points = params->break_points;
  guint64 bin_size = params->bin_size;
  gboolean use_percent = params->use_percent;
  size_t order = GPOINTER_TO_SIZE(data);
  guint64 cache_size = order * bin_size;
  guint64 *progress = params->progress;
  draw_dict *dd = params->dd;


  gint64 *stack_dist = params->stack_dist;
  gint64 *future_stack_dist = params->future_stack_dist;
  cache_t *cache = params->cache->cache_init(cache_size,
                                                   params->cache->obj_id_type,
                                                   params->cache->cache_init_params);

  guint64 *effective_cache_size = malloc(sizeof(guint64) * reader_thread->base->n_total_req);


  // create request struct and initialization
  request_t *req = new_request(cache->obj_id_type);


  guint64 current_effective_size = 0;
  gint64 last_ts = 0;
  guint64 cur_ts = 0;
  gpointer item;
  GHashTable *last_access_time_ght;

  if (req->obj_id_type == OBJ_ID_NUM) {
    last_access_time_ght = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, \
//                                                 g_free,
                                                 NULL);
  } else { // (req->obj_id_type == OBJ_ID_STR)
    last_access_time_ght = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free,
                                                 NULL);
  }

  gboolean increase_b;


  read_one_req(reader_thread, req);

  while (req->valid) {
    // record last access
    if (req->obj_id_type == OBJ_ID_NUM) {
//      obj_id = (gpointer) g_new(guint64, 1);
//      *(guint64 *) obj_id = *(guint64 *) (req->obj_id_ptr);
      item = req->obj_id_ptr;
    } else {
      item = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
    }
    g_hash_table_insert(last_access_time_ght, item, GSIZE_TO_POINTER(cur_ts + 1));


    // add to cache
    if (cache->check(cache, req)) {
      DEBUG("ts %ld, read %s hit\n", (long) cur_ts, (char *) (req->obj_id_ptr));
      cache->_update(cache, req);
      increase_b = FALSE;
    } else {
      DEBUG("ts %ld, read %s miss\n", (long) cur_ts, (char *) (req->obj_id_ptr));
      cache->_insert(cache, req);
      increase_b = TRUE;
    }


    if ((stack_dist[cur_ts] == -1 || stack_dist[cur_ts] >= (gint64) cache_size)) {
      if (!increase_b) {
        ERROR("increase disagree\n");
        abort();
      }
      current_effective_size += 1;
      DEBUG("ts %ld add one %ld (size %ld)\n", (long) cur_ts, (long) current_effective_size, (long) cache_size);
    } else {
      if (increase_b) {
        ERROR("increase disagree\n");
        abort();
      }
    }

    if (future_stack_dist[cur_ts] == -1 || future_stack_dist[cur_ts] >= (gint64) cache_size) {
      current_effective_size -= 1;
      DEBUG("ts %ld reduce one due to %ld %ld\n", (long) cur_ts, (long) future_stack_dist[cur_ts],
            (long) current_effective_size);
    }

//            if (current_effective_size > cache_size){
//                ERROR("ts %lu current eff size %lu, cache size %lu\n", cur_ts, current_effective_size, cache_size);
//                abort();
//            }


    /** check whether the cache is full,
     *  if full, then we need evict, however, this eviction should happen long time ago
     *  in other words, this item should not be added to cache
     *  so need to find out when this item was added and reduced the size of all time after it
     */
    while ((long) cache->used_size > cache->size) {
      DEBUG("ts %ld size %ld %ld\n", (long) cur_ts, (long) cache->used_size,
            (long) (cache->size));
      item = cache->evict_with_return(cache, req);
      last_ts = GPOINTER_TO_UINT(g_hash_table_lookup(last_access_time_ght, item)) - 1;
      if (last_ts < 0) {
        ERROR("last access time < 0, value %ld\n", (long) last_ts);
        abort();
      }
      DEBUG("ts %ld evict %s last access time %ld\n", (long) cur_ts, (char *) item, (long) last_ts);

      if (future_stack_dist[last_ts] != -1 && future_stack_dist[last_ts] < (gint64) cache_size) {
        current_effective_size -= 1;
        DEBUG("ts %ld last access %ld, size %ld %ld, reduce one %ld\n",
              (long) cur_ts, (long) future_stack_dist[last_ts],
              (long) cache->used_size,
              (long) (gint64) cache_size, (long) current_effective_size);
      }
      g_free(item);
    }

    // not cur_ts - 1 because ts has not been increased
    effective_cache_size[cur_ts] = current_effective_size;

    if (current_effective_size > cache_size) {
      ERROR("ts %lu effective size %lu, cache size %lu\n",
            (unsigned long) cur_ts, (unsigned long) current_effective_size,
            (unsigned long) (order * bin_size));
      abort();
    }

    cur_ts += 1;
    read_one_req(reader_thread, req);
  }


//        for (i=0; i<(guint64)(reader_thread->base->n_total_req); i++){
//            if (!(STACK_DIST[i] >= 0 && STACK_DIST[i] < (gint64) cache_size)) {
//                current_effective_size += 1;
//                in_use_cache_size += 1;
//            }
//            if (in_use_cache_size > cache_size){
//                in_use_cache_size --;
//                current_effective_size --;
//            }
//
//            if (future_stack_dist[i] == -1 || future_stack_dist[i] >= (gint64) cache_size)
//                current_effective_size -= 1;
//
//            if (current_effective_size > cache_size){
//                ERROR("ts %lu current eff size %lu, cache size %lu, in use %lu\n", i, current_effective_size, cache_size, in_use_cache_size);
//                abort();
//            }
//
//            effective_cache_size[i] = current_effective_size;
//        }


  // now calculate the stat information
  guint64 sum_effective_size = 0;
  for (i = 0; i < break_points->len - 1; i++) {
    sum_effective_size = 0;
    for (j = g_array_index(break_points, guint64, i); j < g_array_index(break_points, guint64, i + 1); j++) {
      sum_effective_size += effective_cache_size[j];
    }
    dd->matrix[i][order - 1] = (double) (sum_effective_size) /
                               (g_array_index(break_points, guint64, i + 1) - g_array_index(break_points, guint64, i));

    if (dd->matrix[i][order - 1] / cache_size > 1) {
      ERROR("cache size %lu, %lu th bp, effective size %lf\n",
            (unsigned long) cache_size, (unsigned long) i, dd->matrix[i][order - 1]);
      for (j = g_array_index(break_points, guint64, i); j < g_array_index(break_points, guint64, i + 1); j++)
        printf("effective size at %lu (%lu-%lu): %lu\n", (unsigned long) j,
               (unsigned long) g_array_index(break_points, guint64, i),
               (unsigned long) g_array_index(break_points, guint64, i + 1),
               (unsigned long) effective_cache_size[j]);
      abort();
    }

    if (use_percent)
      dd->matrix[i][order - 1] = dd->matrix[i][order - 1] / cache_size;
  }


  // clean up
  g_mutex_lock(&(params->mtx));
  (*progress)++;
  g_mutex_unlock(&(params->mtx));
  g_hash_table_destroy(last_access_time_ght);
  close_cloned_reader(reader_thread);
}


#ifdef __cplusplus
}
#endif
