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





/***********************************************************
 * get heatmap plot data for the stack distance (number of uniq objects) since last access,
 * it returns an array of n_req, where the nth element is the
 * stack distance (number of unique obj) since last access for this obj
 * @param reader
 * @return
 */
heatmap_plot_matrix_t *get_stack_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base);


/***********************************************************
 * get heatmap plot data for the distance since last access,
 * it returns an array of n_req, where the nth element is the
 * distance (number of obj) since last access for this object
 * @param reader
 * @return
 */
heatmap_plot_matrix_t *get_last_access_dist_heatmap_matrix(reader_t *reader, gint32 window, double log_base);




#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_DISTHEATMAP_H
