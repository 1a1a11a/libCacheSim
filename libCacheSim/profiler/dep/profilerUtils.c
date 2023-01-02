//
//  profilerUtils.c
//  libCacheSim
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//



#include <math.h>
#include "../include/libCacheSim/csvReader.h"
#include "../include/libCacheSim/binaryReader.h"
#include "../include/libCacheSim/profilerUtils.h"


#ifdef __cplusplus
extern "C"
{
#endif




    double get_log_base(guint64 max, guint64 expect_result){

        double base = 10;
        double result, prev_result = expect_result;
        while (1){
            result = log((double)max)/log(base);
            if (result>expect_result && prev_result<expect_result)
                return base;
            prev_result = result;
            base = (base - 1)/2 + 1;
        }
    }

    static inline gint process_one_element_last_access(request_t* req,
                                                       GHashTable* hash_table,
                                                       guint64 ts);

    /*-----------------------------------------------------------------------------
     *
     * get_last_access_dist_seq --
     *      this function returns how far away one request was requested in the past,
     *      if it was not requested before, then -1;
     *      it can be run forward or backward depends on the paaed reading func
     *      when running forward, it gives how far in the past of its last request,
     *      when running backward, it gives how far away in the future
     *      it will be requested again.
     *      ATTENTION: when running backward, the returned list is also REVERSED
     *
     * Potential bug:
     *      this function currently using int, may cause some problem when the
     *      trace file is tooooooo large
     *
     * Input:
     *      reader:         the reader for data
     *      funcPtr:        how to read data, can be read_one_element or
     *                      read_one_element_above
     *
     * Return:
     *      GSList* contains the distance to last access
     *
     *-----------------------------------------------------------------------------
     */

    GSList* get_last_access_dist_seq(reader_t* reader,
                                     void (*funcPtr)(reader_t*, request_t*)){


        GSList* list= NULL;

        if (reader->base->total_num == -1)
            get_num_of_req(reader);

        // req struct creation and initialization
        request_t* req = new_req_struct(reader->base->obj_id_type);
        req->block_unit_size = (size_t) reader->base->block_unit_size;

        // create hashtable
        GHashTable * hash_table;
        if (req->obj_id_type == OBJ_ID_NUM){
            hash_table = g_hash_table_new_full(g_int64_hash, g_int64_equal, \
                                               (GDestroyNotify)g_free, \
                                               (GDestroyNotify)g_free);
        }
        else if (req->obj_id_type == OBJ_ID_STR){
            hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, \
                                               (GDestroyNotify)g_free, \
                                               (GDestroyNotify)g_free);
        }
        else{
            ERROR("unknown data obj_id_type: %c\n", req->obj_id_type);
            abort();
        }

        guint64 ts = 0;
        gint dist;

        if (funcPtr == read_one_element){
            read_one_element(reader, req);
        }
        else if (funcPtr == read_one_element_above){
            reader_set_read_pos(reader, 1.0);
            if (go_back_one_req(reader)!=0)
                ERROR("error when going back one line\n");
            read_one_element(reader, req);
        }
        else{
            ERROR("unknown function pointer received in heatmap\n");
            abort();
        }

        while (req->valid){
            dist = process_one_element_last_access(req, hash_table, ts);
            list = g_slist_prepend(list, GSIZE_TO_POINTER(dist));
            funcPtr(reader, req);
            ts++;
        }
        if (reader->base->trace_type == CSV_TRACE){
            csv_params_t *params = reader->reader_params;
            if (params->has_header)
                list = g_slist_remove(list, list->data);
        }


        // clean up
        free_request(req);
        g_hash_table_destroy(hash_table);
        reset_reader(reader);
        return list;
    }


    /*-----------------------------------------------------------------------------
     *
     * process_one_element_last_access --
     *      this function is called by get_last_access_dist_seq,
     *      it insert current request and return distance to its last request
     *
     * Potential bug:
     *      No
     *
     * Input:
     *      req:             request_t contains current request
     *      hash_table:     the hashtable for remembering last access
     *      ts:             current timestamp
     *
     *
     * Return:
     *      distance to last request
     *
     *-----------------------------------------------------------------------------
     */

    static inline gint process_one_element_last_access(request_t* req,
                                                       GHashTable* hash_table,
                                                       guint64 ts){
        gpointer gp;
        gp = g_hash_table_lookup(hash_table, req->obj_id);
        gint ret;
        if (gp == NULL){
            // first time access
            ret = -1;
            guint64* value = g_new(guint64, 1);
            *value = ts;
            if (req->obj_id_type == OBJ_ID_STR)
                g_hash_table_insert(hash_table,
                                    g_strdup((gchar*)(req->obj_id_ptr)),
                                    (gpointer)value);

            else if (req->obj_id_type == OBJ_ID_NUM){
                guint64* key = g_new(guint64, 1);
                *key = *(guint64*)(req->obj_id_ptr);
                g_hash_table_insert(hash_table,
                                    (gpointer)(key),
                                    (gpointer)value);
            }
            else{
                ERROR("unknown request content obj_id_type: %c\n", req->obj_id_type);
                exit(1);
            }
        }
        else{
            // not first time access
            guint64 old_ts = *(guint64*)gp;
            ret = (gint) (ts - old_ts);
            *(guint64*)gp = ts;
        }
        return ret;
    }


    GArray* get_bp_vtime(reader_t* reader,
                         gint64 time_interval,
                         gint64 num_of_piexls){
        /*
         return a GArray of break points, including the last break points
         */

        if (reader->base->total_num == -1)
            get_num_of_req(reader);

        if (reader->sdata->break_points){
            if (reader->sdata->break_points->mode == 'v' &&
                (long) reader->sdata->break_points->time_interval == time_interval )
                return reader->sdata->break_points->array;
            else{
                g_array_free(reader->sdata->break_points->array, TRUE);
                free(reader->sdata->break_points);
            }
        }

        gint i;
        gint array_size = (gint) num_of_piexls;
        if (array_size==-1)
            array_size = (gint) ceil((double) reader->base->total_num/time_interval + 1);
        else
            time_interval = (gint) ceil((double) reader->base->total_num/num_of_piexls + 1);
        //    array_size ++ ;

        GArray* break_points = g_array_new(FALSE, FALSE, sizeof(guint64));
        gint64 value = 0;
        i = 0;
        while (value <= reader->base->total_num){
            g_array_append_val(break_points, value);
            i += 1;
            value = i * time_interval;
        }
        if (value != reader->base->total_num)
            g_array_append_val(break_points, reader->base->total_num);


        if (break_points->len > 10000){
            WARN("number of pixels in one dimension is larger than 10000, "
                    "exact size: %d, it may take a very long time, if you didn't "
                    "intend to do it, please try with a larger time stamp\n",
                    break_points->len);
        }
        else if (break_points->len < 20){
            WARN("number of pixels in one dimension is smaller than 20, "
                    "exact size: %d, each pixel will be very large, if you didn't "
                    "intend to do this, please try with a smaller time stamp\n",
                    break_points->len);
        }

        struct break_point* bp = g_new(struct break_point, 1);
        bp->mode = 'v';
        bp->time_interval = time_interval;
        bp->array = break_points;
        reader->sdata->break_points = bp;

        reset_reader(reader);
        return break_points;
    }


    GArray* get_bp_rtime(reader_t* reader,
                         gint64 time_interval,
                         gint64 num_of_piexls){
        /*
         currently this only works for vscsi reader !!!
         return a GArray of break points, including the last break points
         */
        if (reader->base->trace_type == PLAIN_TXT_TRACE){
            ERROR("get_bp_rtime currently don't support plain reader, program exit\n");
            abort();
        }
        if (reader->base->trace_type == CSV_TRACE){
            csv_params_t* params = reader->reader_params;
            if (params->real_time_column == -1 || params->real_time_column == 0){
                ERROR("get_bp_rtime needs you to provide "
                      "real_time_column parameter for csv reader\n");
                exit(1);
            }
        }
        if (reader->base->trace_type == BIN_TRACE){
            binary_params_t* params = reader->reader_params;
            if (params->real_time_pos == 0){
                ERROR("get_bp_rtime needs you to provide "
                      "clock_time parameter for binary reader\n");
                exit(1);
            }
        }


        if (reader->base->total_num == -1)
            get_num_of_req(reader);


        if (reader->sdata->break_points){
            if (reader->sdata->break_points->mode == 'r' &&
                (long) reader->sdata->break_points->time_interval == time_interval ){
                return reader->sdata->break_points->array;
            }
            else{
                g_array_free(reader->sdata->break_points->array, TRUE);
                free(reader->sdata->break_points);
            }
        }

        guint64 previous_time = 0;
        GArray* break_points = g_array_new(FALSE, FALSE, sizeof(guint64));

        // create request struct and initialization
        request_t* req = new_req_struct(reader->base->obj_id_type);

        guint64 num = 0;

        reset_reader(reader);
        read_one_element(reader, req);
        previous_time = req->clock_time;
        g_array_append_val(break_points, num);



        if (num_of_piexls != -1 && time_interval == -1){
            reader_set_read_pos(reader, 1);
            read_one_element_above(reader, req);
            time_interval = (gint64) ceil( (double)(req->clock_time - previous_time) /num_of_piexls + 1);
            reader_set_read_pos(reader, 0);
            read_one_element(reader, req);
        }


        while (req->valid){
            if (req->clock_time - previous_time > (guint64)time_interval){
                g_array_append_val(break_points, num);
                previous_time = req->clock_time;
            }
            read_one_element(reader, req);
            num++;
        }
        if ((gint64)g_array_index(break_points, guint64, break_points->len-1) != reader->base->total_num)
            g_array_append_val(break_points, reader->base->total_num);


        if (break_points->len > 10000){
            WARN("number of pixels in one dimension is larger than 10000, "
                    "exact size: %d, it may take a very long time, if you didn't "
                    "intend to do it, please try with a larger time stamp\n",
                    break_points->len);
        }
        else if (break_points->len < 20){
            WARN("number of pixels in one dimension is smaller than 20, "
                    "exact size: %d, each pixel will be very large, if you didn't "
                    "intend to do this, please try with a smaller time stamp\n",
                    break_points->len);
        }

        struct break_point* bp = g_new(struct break_point, 1);
        bp->mode = 'r';
        bp->time_interval = time_interval;
        bp->array = break_points;
        reader->sdata->break_points = bp;

        // clean up
        free_request(req);
        reset_reader(reader);
        return break_points;
    }


#ifdef __cplusplus
}
#endif
