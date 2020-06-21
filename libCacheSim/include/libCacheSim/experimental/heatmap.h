//
// This module is used only by some internal heatmap computation module
//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef libCacheSim_HEATMAP_H
#define libCacheSim_HEATMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double **matrix;
  gint32 xlim; // n_window
  gint32 ylim; // n_pts
  double log_scale;
} heatmap_plot_matrix_t;

static inline heatmap_plot_matrix_t *
new_heatmap_plot_matrix(gint32 xlim, gint32 ylim, double log_scale) {
  heatmap_plot_matrix_t *hm_matrix = g_new0(heatmap_plot_matrix_t, 1);
  hm_matrix->xlim = xlim;
  hm_matrix->ylim = ylim;
  hm_matrix->log_scale = log_scale;

  hm_matrix->matrix = g_new0(double *, xlim);
  for (gint32 i = 0; i < xlim; i++)
    hm_matrix->matrix[i] = g_new0(double, ylim);

  return hm_matrix;
}

static inline void free_heatmap_plot_matrix(heatmap_plot_matrix_t *hm_matrix) {
  for (gint32 i = 0; i < hm_matrix->xlim; i++) {
    g_free(hm_matrix->matrix[i]);
  }
  g_free(hm_matrix->matrix);
  g_free(hm_matrix);
}

static inline void print_heatmap_plot_matrix(heatmap_plot_matrix_t *hm_matrix) {
  for (gint32 i = 0; i < hm_matrix->xlim; i++) {
    for (gint32 j = 0; j < hm_matrix->ylim - 1; j++) {
      printf("%.2f, ", hm_matrix->matrix[i][j]);
    }
    printf("%.2f\n", hm_matrix->matrix[i][hm_matrix->ylim - 1]);
  }
}

static inline void normalize_heatmap_matrix(heatmap_plot_matrix_t *hm_matrix) {
  gint64 sum = 0;
  for (gint32 i = 0; i < hm_matrix->xlim; i++) {
    sum = 0;
    for (gint32 j = 0; j < hm_matrix->ylim; j++) {
      sum += hm_matrix->matrix[i][j];
    }
    for (gint32 j = 0; j < hm_matrix->ylim; j++) {
      hm_matrix->matrix[i][j] /= sum;
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_HEATMAP_H
