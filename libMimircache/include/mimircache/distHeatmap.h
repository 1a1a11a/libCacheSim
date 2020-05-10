//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef LIBMIMIRCACHE_DISTHEATMAP_H
#define LIBMIMIRCACHE_DISTHEATMAP_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "reader.h"
#include "const.h"
#include "heatmapUtils.h"





/**
 * calculate the heatmap plot data of stack distance distribution
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t *get_stack_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base);


/**
 * calculate the heatmap plot data of distance from last access distribution
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t *get_last_access_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base);



/**
 * get heatmap plot data for reues time distribution in each time window
 * @param reader
 * @param window
 * @param log_base
 * @return
 */
heatmap_plot_matrix_t * get_reuse_time_heatmap_matrix(reader_t *reader, gint32 window, double log_base);




#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_DISTHEATMAP_H
