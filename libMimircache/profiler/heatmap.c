//
//  heatmap.c
//  mimircache
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "../include/mimircache/heatmap.h"
#include "Optimal.h"
#include "FIFO.h"


#ifdef __cplusplus
extern "C"
{
#endif


draw_dict *heatmap_computation(reader_t *reader,
                               cache_t *cache,
                               char time_mode,
                               heatmap_type_e plot_type,
                               hm_comp_params_t *hm_comp_params,
                               int num_of_threads);


draw_dict *hm_hr_st_et(reader_t *reader,
                       cache_t *cache,
                       gboolean interval_hit_ratio_b,
                       double decay_coefficient_lf,
                       int num_of_threads);

/*-----------------------------------------------------------------------------
 *
 * heatmap --
 *      the entrance for heatmap computation
 *
 * Input:
 *      reader:                 the reader for data
 *      cache:                  cache which heatmap is based on
 *      time_mode:              real time (r) or virtual time (v)
 *      time_interval:          real time interval or vritual time interval
 *      num_of_pixels:          the number of pixels in x/y dimension,
 *                                  this is optional, if time_interval is specified,
 *                                  then this one is not needed
 *      plot_type:              the obj_id_type of plot
 *
 *      interval_hit_ratio_b:   used in hit_ratio_start_time_end_time,
 *                                  if it is True, then the hit ratio of each pixel is not
 *                                  the average hit ratio from beginning,
 *                                  instead it is a combined hit ratio of exponentially decayed
 *                                  average hit ratio plus hit ratio in current interval
 *      decay_coefficient_lf:      used only when interval_hit_ratio_b is True
 *
 *      num_of_threads: the maximum number of threads can use
 *
 * Return:
 *      draw_dict
 *
 *-----------------------------------------------------------------------------
 */

draw_dict *heatmap(reader_t *reader,
                   cache_t *cache,
                   char time_mode,
                   gint64 time_interval,
                   gint64 num_of_pixel_for_time_dim,
                   heatmap_type_e plot_type,
                   hm_comp_params_t *hm_comp_params,
                   int num_of_threads) {

  if (time_mode == 'v')
    get_bp_vtime(reader, time_interval, num_of_pixel_for_time_dim);
  else if (time_mode == 'r')
    get_bp_rtime(reader, time_interval, num_of_pixel_for_time_dim);
  else {
    ERROR("unsupported mode: %c\n", time_mode);
    abort();
  }


  // check cache is LRU or not
  if (cache == NULL || strcmp(cache->core->cache_name, "LRU") == 0) {
    if (plot_type != future_rd_distribution &&
        plot_type != dist_distribution &&
        plot_type != rt_distribution &&
        plot_type != effective_size) {
      if (reader->sdata->reuse_dist_type != REUSE_DIST) {
        _get_reuse_dist_seq(reader);
      }
    }
  }
  return heatmap_computation(reader, cache, time_mode,
                             plot_type, hm_comp_params, num_of_threads);

}


/*-----------------------------------------------------------------------------
 *
 * heatmap_LRU --
 *      heatmap computation for LRU
 *
 * Input:
 *      reader:         the reader for data
 *      cache:          cache which heatmap is based on, contains information
 *                      including cache_size, etc.
 *      time_mode:           real time (r) or virtual time (v)
 *      time_interval:  real time interval or vritual time interval
 *      num_of_pixels:  the number of pixels in x/y dimension,
 *                      this is optional, if time_interval is specified,
 *                      then this one is not needed
 *      plot_type:      the obj_id_type of plot
 *
 *
 *      inside hm_comp_params:
 *          interval_hit_ratio_b:   used in hit_ratio_start_time_end_time,
 *                                  if it is True, then the hit ratio of each pixel is not
 *                                  the average hit ratio from beginning,
 *                                  instead it is a combined hit ratio of exponentially decayed
 *                                  average hit ratio plus hit ratio in current interval
 *          decay_coefficient_lf:      used only when interval_hit_ratio_b is True
 *          use_percent:            whether the final result is in the form of percent of raw number
 *                                  currently used in opt_effective_size
 *
 *      num_of_threads: the maximum number of threads can use
 *
 * Return:
 *      draw_dict
 *
 *-----------------------------------------------------------------------------
 */

draw_dict *heatmap_computation(reader_t *reader,
                               cache_t *cache,
                               char time_mode,
                               heatmap_type_e plot_type,
                               hm_comp_params_t *hm_comp_params,
                               int num_of_threads) {
  if (plot_type == hr_st_et) {
    WARNING("this has been changed not sure it is correct or not\n");
    if (reader->sdata->reuse_dist_type != LAST_DIST) {
      g_free(reader->sdata->reuse_dist);
      reader->sdata->reuse_dist = NULL;
      reader->sdata->reuse_dist = get_last_access_dist(reader);
    }
    return hm_hr_st_et(reader, cache,
                       hm_comp_params->interval_hit_ratio_b,
                       hm_comp_params->ewma_coefficient_lf,
                       num_of_threads);
  } else if (plot_type == rd_distribution) {
    return heatmap_rd_distribution(reader, time_mode, num_of_threads, 0);
  } else if (plot_type == rd_distribution_CDF) {
    return heatmap_rd_distribution(reader, time_mode, num_of_threads, 1);
  } else if (plot_type == future_rd_distribution) {
    if (reader->sdata->reuse_dist_type != FUTURE_RD) {
      g_free(reader->sdata->reuse_dist);
      reader->sdata->reuse_dist = NULL;
      reader->sdata->reuse_dist = get_future_reuse_dist(reader);
    }

    draw_dict *dd = heatmap_rd_distribution(reader, time_mode, num_of_threads, 0);

    reader->sdata->max_reuse_dist = 0;
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;

    return dd;
  } else if (plot_type == dist_distribution) {
    if (reader->sdata->reuse_dist_type != LAST_DIST) {
      g_free(reader->sdata->reuse_dist);
      reader->sdata->reuse_dist = NULL;
      reader->sdata->reuse_dist = get_last_access_dist(reader);
    }

    draw_dict *dd = heatmap_rd_distribution(reader, time_mode, num_of_threads, 0);
    reader->sdata->max_reuse_dist = 0;
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;

    return dd;
  } else if (plot_type == rt_distribution) {        // real time distribution, time msut be integer
    if (reader->sdata->reuse_dist_type != REUSE_TIME) {
      g_free(reader->sdata->reuse_dist);
      reader->sdata->reuse_dist = NULL;
      reader->sdata->reuse_dist = get_reuse_time(reader);
    }

    draw_dict *dd = heatmap_rd_distribution(reader, time_mode, num_of_threads, 0);
    reader->sdata->max_reuse_dist = 0;
    g_free(reader->sdata->reuse_dist);
    reader->sdata->reuse_dist = NULL;

    return dd;
  } else {
    ERROR("unknown plot obj_id_type %d\n", plot_type);
    exit(1);
  }
}


/** heatmap_hit_ratio_start_time_end_time **/

draw_dict *hm_hr_st_et(reader_t *reader,
                       cache_t *cache,
                       gboolean interval_hit_ratio_b,
                       double ewma_coefficient_lf,
                       int num_of_threads) {

  gint i;
  guint64 progress = 0;

  GArray *break_points;
  break_points = reader->sdata->break_points->array;

  // create draw_dict storage
  draw_dict *dd = g_new(draw_dict, 1);
  dd->xlength = break_points->len - 1;
  dd->ylength = break_points->len - 1;
  dd->matrix = g_new(double*, dd->xlength);
  for (i = 0; i < (gint) (dd->xlength); i++)
    dd->matrix[i] = g_new0(double, dd->ylength);


  // build parameters
  mt_params_hm_t *params = g_new(mt_params_hm_t, 1);
  params->reader = reader;
  params->break_points = break_points;
  params->cache = cache;
  params->dd = dd;
  params->interval_hit_ratio_b = interval_hit_ratio_b;
  params->ewma_coefficient_lf = ewma_coefficient_lf;
  params->progress = &progress;
  g_mutex_init(&(params->mtx));


  // build the thread pool
  GThreadPool *gthread_pool;
  if (strcmp(cache->core->cache_name, "LRU") == 0)
    gthread_pool = g_thread_pool_new((GFunc) hm_LRU_hr_st_et_thread,
                                     (gpointer) params, num_of_threads, TRUE, NULL);
  else
    gthread_pool = g_thread_pool_new((GFunc) hm_nonLRU_hr_st_et_thread,
                                     (gpointer) params, num_of_threads, TRUE, NULL);

  if (gthread_pool == NULL) ERROR("cannot create thread pool in heatmap\n");


  // send data to thread pool and begin computation
  for (i = break_points->len - 2; i >= 0; i--) {
    if (g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i + 1), NULL) == FALSE)  // +1 otherwise, 0 will be a problem
    ERROR("cannot push data into thread in generalprofiler\n");
  }

  // this is to prevent entering loop when computation is short
  sleep(2);
  while (progress < break_points->len - 1) {
    sleep(5);
    fprintf(stderr, "%.2f%%\n", ((double) progress) / break_points->len * 100);
//            fprintf(stderr, "\033[A\033[2K\r");
  }

  g_thread_pool_free(gthread_pool, FALSE, TRUE);


  g_mutex_clear(&(params->mtx));
  g_free(params);
  // needs to free draw_dict later
  return dd;
}


draw_dict *heatmap_rd_distribution(reader_t *reader, char mode, int num_of_threads, int CDF) {
  /* Do NOT call this function directly,
   * call top level heatmap, which setups some data for this function */

  /* this one, the result is in the log form */

  guint i;
  guint64 progress = 0;

  GArray *break_points;
  break_points = reader->sdata->break_points->array;

  // this is used to make sure length of x and y are approximate same, not different by too much
  if (reader->sdata->max_reuse_dist == 0 || break_points->len == 0) {
    ERROR("did you call top level function? max reuse distance %ld, bp len %u\n",
          (long) reader->sdata->max_reuse_dist, break_points->len);
    exit(1);
  }
  double log_base = get_log_base((unsigned long) reader->sdata->max_reuse_dist, break_points->len);
  reader->udata->log_base = log_base;

  // create draw_dict storage
  draw_dict *dd = g_new(draw_dict, 1);
  dd->xlength = break_points->len - 1;

  // the last one is used for store cold miss; REUSE_DIST=0 and REUSE_DIST=1 are combined at first bin (index=0)
  dd->ylength = (guint64) ceil(log((double) reader->sdata->max_reuse_dist) / log(log_base));
  dd->matrix = g_new(double*, break_points->len);
  for (i = 0; i < dd->xlength; i++)
    dd->matrix[i] = g_new0(double, dd->ylength);


  // build parameters
  mt_params_hm_t *params = g_new(mt_params_hm_t, 1);
  params->reader = reader;
  params->break_points = break_points;
  params->dd = dd;
  params->progress = &progress;
  params->log_base = log_base;
  g_mutex_init(&(params->mtx));


  // build the thread pool
  GThreadPool *gthread_pool;
  if (!CDF)
    gthread_pool = g_thread_pool_new((GFunc) hm_rd_distribution_thread,
                                     (gpointer) params, num_of_threads, TRUE, NULL);
  else
    gthread_pool = g_thread_pool_new((GFunc) hm_rd_distribution_CDF_thread,
                                     (gpointer) params, num_of_threads, TRUE, NULL);

  if (gthread_pool == NULL) ERROR("cannot create thread pool in heatmap rd_distribution\n");


  // send data to thread pool and begin computation
  for (i = 0; i < break_points->len - 1; i++) {
    if (g_thread_pool_push(gthread_pool, GSIZE_TO_POINTER(i + 1), NULL) == FALSE)  // +1, otherwise, 0 will be a problem
    ERROR("cannot push data into thread in generalprofiler\n");
  }

  // this is to prevent entering loop when computation is short
  sleep(2);
  while (progress < break_points->len - 1) {
    sleep(5);
    fprintf(stderr, "%.2lf%%\n", ((double) progress) / break_points->len * 100);
//            fprintf(stderr, "\033[A\033[2K\r");
  }

  g_thread_pool_free(gthread_pool, FALSE, TRUE);


  g_mutex_clear(&(params->mtx));
  g_free(params);

  return dd;
}



void free_draw_dict(draw_dict *dd) {
  guint64 i;
  for (i = 0; i < dd->xlength; i++) {
    g_free(dd->matrix[i]);
  }
  g_free(dd->matrix);
  g_free(dd);
}


#ifdef __cplusplus
}
#endif
