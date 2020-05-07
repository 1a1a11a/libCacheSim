//
// Created by Juncheng Yang on 5/7/20.
//

#include <math.h>
#include "../include/mimircache/distUtils.h"
#include "../include/mimircache/distHeatmap.h"
#include "../readerUtils/include/readerUtils.h"
#include "../utils/include/mathUtils.h"


heatmap_plot_matrix_t *get_stack_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base) {
  gint32 n_window, x_pos = 0, y_pos = 0;
  double div = log(log_base);

  gint64* boundary = get_window_boundary(reader, window, &n_window);
  gint64* stack_dist = get_stack_dist(reader);

  gint64 max_sd = 0;
  find_max_gint64(stack_dist, reader->base->n_total_req, &max_sd, NULL);
  gint32 n_pts = (gint32) ceil(log((double) max_sd+1)/log(log_base))+1;
  heatmap_plot_matrix_t *hm_matrix = new_heatmap_plot_matrix(n_window, n_pts, log_base);

  for (gint64 i=0; i<reader->base->n_total_req; i++){
    if (i == boundary[x_pos])
      x_pos += 1;
    if (stack_dist[i] == -1)
      continue;
    y_pos = (gint32) (ceil(log((double) stack_dist[i]+1) / div));
    hm_matrix->matrix[x_pos][y_pos] += 1;
  }
  g_free(boundary);
  g_free(stack_dist);
  return hm_matrix;
}



heatmap_plot_matrix_t * get_last_access_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base){
  gint32 n_window, x_pos = 0, y_pos = 0;
  double div = log(log_base);

  gint64* boundary = get_window_boundary(reader, window, &n_window);
  gint64* dist = get_last_access_dist(reader);

  gint64 max_sd = 0;
  find_max_gint64(dist, reader->base->n_total_req, &max_sd, NULL);
  gint32 n_pts = (gint32) ceil(log((double) max_sd+1)/log(log_base))+1;
  heatmap_plot_matrix_t *hm_matrix = new_heatmap_plot_matrix(n_window, n_pts, log_base);

  for (gint64 i=0; i<reader->base->n_total_req; i++){
    if (i == boundary[x_pos])
      x_pos += 1;
    if (dist[i] == -1)
      continue;
    y_pos = (gint32) (ceil(log((double) dist[i]+1) / div));
    hm_matrix->matrix[x_pos][y_pos] += 1;
  }
  g_free(boundary);
  g_free(dist);
  return hm_matrix;
}
