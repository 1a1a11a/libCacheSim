//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef LIBMIMIRCACHE_HEATMAPUTILS_H
#define LIBMIMIRCACHE_HEATMAPUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct {
  double **matrix;
  gint32 n_window;    // x
  gint32 n_pts;       // y
  double log_scale;
} heatmap_plot_matrix_t;


static inline heatmap_plot_matrix_t *new_heatmap_plot_matrix(gint32 n_window, gint32 n_pts, double log_scale){
  heatmap_plot_matrix_t *hm_matrix = g_new0(heatmap_plot_matrix_t, 1);
  hm_matrix->n_window = n_window;
  hm_matrix->n_pts = n_pts;
  hm_matrix->log_scale = log_scale;

  hm_matrix->matrix = g_new0(double*, n_window);
  for (gint32 i=0; i<n_window; i++)
    hm_matrix->matrix[i] = g_new0(double, n_pts);

  return hm_matrix;
}

static inline void free_heatmap_plot_matrix(heatmap_plot_matrix_t* hm_matrix){
  for (gint32 i=0; i<hm_matrix->n_window; i++){
    g_free(hm_matrix->matrix[i]);
  }
  g_free(hm_matrix->matrix);
  g_free(hm_matrix);
}

static inline void print_heatmap_plot_matrix(heatmap_plot_matrix_t *hm_matrix) {
  for (gint32 i=0; i<hm_matrix->n_window; i++){
    for (gint32 j=0; j<hm_matrix->n_pts-1; j++){
      printf("%.2f, ", hm_matrix->matrix[i][j]);
    }
    printf("%.2f\n", hm_matrix->matrix[i][hm_matrix->n_pts-1]);
  }
}

static inline void normalize_heatmap_matrix(heatmap_plot_matrix_t *hm_matrix) {
  gint64 sum = 0;
  for (gint32 i=0; i<hm_matrix->n_window; i++){
    sum = 0;
    for (gint32 j=0; j<hm_matrix->n_pts-1; j++){
      sum += hm_matrix->matrix[i][j];
    }
    for (gint32 j=0; j<hm_matrix->n_pts-1; j++){
      hm_matrix->matrix[i][j] /= sum;
    }
  }
}


#ifdef __cplusplus
}
#endif



#endif //LIBMIMIRCACHE_HEATMAPUTILS_H
