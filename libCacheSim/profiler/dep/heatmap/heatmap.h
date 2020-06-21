//
//  heatmap.h
//  heatmap
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef heatmap_h
#define heatmap_h

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include "reader.h"
#include "cache.h"
#include "profilerLRU.h"

#include "const.h"
#include "request.h"
#include <math.h>


#ifdef __cplusplus
extern "C"
{
#endif

    
    typedef struct {
        gint64 bin_size_ld;
        
        gboolean interval_hit_ratio_b;
        double ewma_coefficient_lf;
        
        gboolean use_percent_b;

    }hm_comp_params_t;
    
    typedef struct {
        /*
         the data in matrix is stored in column based for performance consideration
         because in each computation thread, the result is stored on one column
         think it as the x, y coordinates
         */

        guint64 xlength;
        guint64 ylength;
        double** matrix;

    }draw_dict;


    typedef struct _multithreading_params_heatmap{
        int order;
        reader_t* reader;
        struct cache* cache;
        guint64 cache_size;
        gint64 bin_size;

        gboolean interval_hit_ratio_b;
        double ewma_coefficient_lf;

        gboolean use_percent; 
        
        gint64 *stack_dist;
        gint64 *future_stack_dist;
        
        GArray* break_points;
        draw_dict* dd;
        guint64* progress;
        GMutex mtx;

        // used in rd_distribution
        double log_base;
    }mt_params_hm_t;


    typedef enum _heatmap_type{
        hr_st_et,
        hr_st_size,
        avg_rd_st_et,
        effective_size,
        rd_distribution,
        future_rd_distribution,
        dist_distribution,          // this one is not using reuse distance, instead using distance to last access
        rt_distribution,            // real time distribution, use integer for time
        rd_distribution_CDF,
        hr_interval_size            // hit ratio of current interval and cache size
    }heatmap_type_e;




    void free_draw_dict(draw_dict* dd);



    draw_dict* heatmap(reader_t* reader,
                       cache_t* cache,
                       char time_mode,
                       gint64 time_interval,
                       gint64 num_of_pixel_for_time_dim,
                       heatmap_type_e plot_type,
                       hm_comp_params_t* hm_comp_params,
                       int num_of_threads);

    draw_dict* differential_heatmap(reader_t* reader,
                                    cache_t* cache1,
                                    cache_t* cache2,
                                    char time_mode,
                                    gint64 time_interval,
                                    gint64 num_of_pixels,
                                    heatmap_type_e plot_type,
                                    hm_comp_params_t* hm_comp_params,
                                    int num_of_threads);

    draw_dict* heatmap_rd_distribution(reader_t* reader,
                                       char time_mode,
                                       int num_of_threads,
                                       int CDF);



    /* heatmap_thread */
    void hm_LRU_hr_st_et_thread(gpointer data,
                                gpointer user_data);

    void hm_nonLRU_hr_st_et_thread(gpointer data,
                                   gpointer user_data);

    void hm_LRU_hr_interval_size_thread(gpointer data,
                                        gpointer user_data);

    void hm_nonLRU_hr_interval_size_thread(gpointer data,
                                           gpointer user_data);


    void hm_rd_distribution_thread(gpointer data,
                                   gpointer user_data);

    void hm_rd_distribution_CDF_thread(gpointer data,
                                       gpointer user_data);

    void hm_effective_size_thread(gpointer data,
                                          gpointer user_data);

    void hm_LRU_effective_size_thread(gpointer data, gpointer user_data); 

        
#ifdef __cplusplus
}
#endif

#endif /* heatmap_h */
