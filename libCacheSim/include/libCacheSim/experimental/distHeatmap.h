//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef libCacheSim_DISTHEATMAP_H
#define libCacheSim_DISTHEATMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../const.h"
#include "../reader.h"
#include "heatmap.h"

/**
 * calculate the heatmap plot data of stack distance distribution
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t *
get_stack_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base);

/**
 * calculate the heatmap plot data of distance from last access distribution
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t *get_last_access_dist_heatmap_matrix(reader_t *reader,
                                                           gint32 window,
                                                           double log_base);

/**
 * get heatmap plot data for reuse time distribution in each time window
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t *
get_reuse_time_heatmap_matrix(reader_t *reader, gint32 window, double log_base);

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_DISTHEATMAP_H
