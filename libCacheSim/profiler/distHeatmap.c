//
// Created by Juncheng Yang on 5/7/20.
//

#include "../include/libCacheSim/experimental/distHeatmap.h"
#include "../include/libCacheSim/macro.h"
#include "../include/libCacheSim/dist.h"
#include "../traceReader/include/readerUtils.h"

#include <math.h>

typedef gint64 *(*dist_func_ptr)(reader_t *);

static heatmap_plot_matrix_t *_get_dist_heatmap_matrix(reader_t *reader, gint32 window,
                                                double log_base,
                                                dist_func_ptr dist_func);

static heatmap_plot_matrix_t *_get_dist_heatmap_matrix(reader_t *reader, gint32 window,
                                                double log_base,
                                                dist_func_ptr dist_func) {
  gint32 n_window, x_pos = 0, y_pos = 0;
  double div = log(log_base);

  gint64 *boundary = get_window_boundary(reader, window, &n_window);
  gint64 *dist = dist_func(reader);

  gint64 max_sd = 0, max_sd_idx = 0;
  find_max(dist, get_num_of_req(reader), &max_sd, &max_sd_idx);
  gint32 n_pts = (gint32)ceil(log((double)max_sd + 1) / log(log_base)) + 1;
  heatmap_plot_matrix_t *hm_matrix =
      new_heatmap_plot_matrix(n_window, n_pts, log_base);

  for (gint64 i = 0; i < get_num_of_req(reader); i++) {
    if (i == boundary[x_pos])
      x_pos += 1;
    if (dist[i] == -1)
      continue;
    y_pos = (gint32)(ceil(log((double)dist[i] + 1) / div));
    hm_matrix->matrix[x_pos][y_pos] += 1;
  }

  normalize_heatmap_matrix(hm_matrix);
  g_free(boundary);
  g_free(dist);
  return hm_matrix;
}

heatmap_plot_matrix_t *get_stack_dist_heatmap_matrix(reader_t *reader,
                                                     gint32 window,
                                                     double log_base) {
  return _get_dist_heatmap_matrix(reader, window, log_base, get_stack_dist);
}

heatmap_plot_matrix_t *get_last_access_dist_heatmap_matrix(reader_t *reader,
                                                           gint32 window,
                                                           double log_base) {
  return _get_dist_heatmap_matrix(reader, window, log_base,
                                  get_last_access_dist);
}

heatmap_plot_matrix_t *get_reuse_time_heatmap_matrix(reader_t *reader,
                                                     gint32 window,
                                                     double log_base) {
  return _get_dist_heatmap_matrix(reader, window, log_base, get_reuse_time);
}
