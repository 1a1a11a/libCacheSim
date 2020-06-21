

//
//  heatmapTest.c
//  test
//
//  Created by Jason on 2/20/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//



#include "../../libCacheSim/include/libCacheSim/experimental/heatmap.h"

#include "../../libCacheSim/include/libCacheSim/reader.h"
#include "../libCacheSim/include/libCacheSim/csvReader.h"

#include "../libCacheSim/cacheAlg/dep/LRU.h"
#include "../libCacheSim/cacheAlg/dep/FIFO.h"

#include <glib.h>


#define CACHESIZE 2000
#define BIN_SIZE (CACHESIZE)
#define TIME_INTERVAL 120 * 1000000
#define MODE 'r'

#define BLOCK_UNIT_SIZE 0    // 16 * 1024
#define DISK_SECTOR_SIZE 0  // 512


void heatmapTest() {

    draw_dict* dd ;
    cache_t* fifo = fifo_init(CACHESIZE, OBJ_ID_NUM, NULL);
    struct optimal_init_params* opt_init_params = g_new0(struct optimal_init_params, 1);
    opt_init_params->next_access = NULL;
    opt_init_params->reader = reader;
    cache_t* optimal = optimal_init(CACHESIZE, OBJ_ID_NUM, BLOCK_UNIT_SIZE, (void*)opt_init_params);
    cache_t* lru = LRU_init(CACHESIZE, OBJ_ID_NUM, BLOCK_UNIT_SIZE, NULL);

    hm_comp_params_t hm_comp_params;
    hm_comp_params.bin_size_ld = BIN_SIZE;
    hm_comp_params.interval_hit_ratio_b = FALSE;
    hm_comp_params.ewma_coefficient_lf = 0;
    hm_comp_params.use_percent_b = FALSE;
    

    guint n_threads = get_n_cores();
    if (n_threads > 4){
        n_threads = 4;
        INFO("use %u threads\n", n_threads);
    }
    
    
    // TRUE DATA
    guint64 bp_len = 355;

    g_assert_cmpuint(get_bp_rtime(reader, 20 * 1000000, -1)->len, ==, bp_len);


    dd = heatmap(reader, lru, 'r', 2000*1000000, 2000, hr_st_et, &hm_comp_params, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 4);
    g_assert_cmpuint(dd->ylength, ==, 4);
    g_assert_cmpfloat(fabs(dd->matrix[0][0]-0.335128), <, 0.01);
    free_draw_dict(dd);
    
    dd = heatmap(reader, optimal, 'v', 2000, 2000, hr_interval_size, &hm_comp_params, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 57);
    g_assert_cmpuint(dd->ylength, ==, 13);
    g_assert_cmpfloat(fabs(dd->matrix[2][2] - 0.645000), <, 0.01);
    free_draw_dict(dd);
    
    dd = heatmap(reader, NULL, 'r', 2000*1000000, -1, rd_distribution_CDF, NULL, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 4);
    g_assert_cmpuint(dd->ylength, ==, 7);
    g_assert_cmpfloat(fabs(dd->matrix[0][0] - 0.693074), <, 0.01);
    free_draw_dict(dd);

    dd = heatmap(reader, NULL, 'r', 2000*1000000, -1, rd_distribution, NULL, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 4);
    g_assert_cmpuint(dd->ylength, ==, 7);
    g_assert_cmpfloat(fabs(dd->matrix[0][0] - 34626.00), <, 0.01);
    free_draw_dict(dd);

    dd = heatmap(reader, NULL, 'r', 2000*1000000, -1, future_rd_distribution, NULL, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 4);
    g_assert_cmpuint(dd->ylength, ==, 7);
    g_assert_cmpfloat(fabs(dd->matrix[0][0] - 12015.00), <, 0.01);
    free_draw_dict(dd);

    dd = heatmap(reader, NULL, 'r', 2000*1000000, -1, effective_size, &hm_comp_params, n_threads);
//    g_assert_cmpuint(dd->xlength, ==, 4);
//    g_assert_cmpuint(dd->ylength, ==, 7);
//    g_assert_cmpfloat(fabs(dd->matrix[0][0] - 12015.00), <, 0.01);
    free_draw_dict(dd);

    
    dd = differential_heatmap(reader, lru, fifo, 'r', 2000*1000000, -1, hr_st_et, &hm_comp_params, n_threads);
    g_assert_cmpuint(dd->xlength, ==, 4);
    g_assert_cmpuint(dd->ylength, ==, 4);
    g_assert_cmpfloat(fabs(dd->matrix[0][0] + 0.005316), <, 0.01);
    free_draw_dict(dd);


    cache_destroy(lru);
    fifo->destroy(fifo);
    close_reader(reader);
}

void new_test_func(){
    
    draw_dict* dd ;
    csvReader_init_params* csv_init_params = (void*) new_csvReader_init_params(5, -1, 2, -1, TRUE, ',', -1);
//    reader_t* reader = setup_reader("../data/trace.csv", 'c', OBJ_ID_NUM, BLOCK_UNIT_SIZE, DISK_SECTOR_SIZE, csv_init_params);
    reader_t* reader = setup_reader("../data/trace.txt", 'p', OBJ_ID_NUM, BLOCK_UNIT_SIZE, DISK_SECTOR_SIZE, NULL);
    struct optimal_init_params* opt_init_params = g_new0(struct optimal_init_params, 1);
    opt_init_params->next_access = NULL;
    opt_init_params->reader = reader;
    cache_t* optimal = optimal_init(CACHESIZE, OBJ_ID_NUM, BLOCK_UNIT_SIZE, (void*)opt_init_params);
    cache_t* lru = LRU_init(CACHESIZE, OBJ_ID_NUM, BLOCK_UNIT_SIZE, NULL);
    guint n_threads = get_n_cores();
    if (n_threads > 4){
        n_threads = 4;
        INFO("use %u threads\n", n_threads);
    }
    hm_comp_params_t hm_comp_params;
    hm_comp_params.bin_size_ld = 2000;
    hm_comp_params.use_percent_b = FALSE;
    

    dd = heatmap(reader, lru, 'v', -1, 2000, effective_size, &hm_comp_params, n_threads);

}


int main(int argc, char* argv[]) {
    new_test_func();
//    heatmapTest();
    return 1;
//    g_test_init(&argc, &argv, NULL);
//    g_test_add_func("/test/heatmapTest", heatmapTest);
//    return g_test_run();
}

