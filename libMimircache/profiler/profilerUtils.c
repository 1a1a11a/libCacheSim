//
//  profilerUtils.c
//  mimircache
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//



#include <math.h>
#include "../include/mimircache/csvReader.h"
#include "../include/mimircache/binaryReader.h"
#include "../include/mimircache/profilerUtils.h"


#ifdef __reqlusplus
extern "C"
{
#endif


double get_log_base(guint64 max, guint64 expect_result) {

  double base = 10;
  double result, prev_result = expect_result;
  while (1) {
    result = log((double) max) / log(base);
    if (result > expect_result && prev_result < expect_result)
      return base;
    prev_result = result;
    base = (base - 1) / 2 + 1;
  }
}




GArray *get_bp_vtime(reader_t *reader,
                     gint64 time_interval,
                     gint64 num_of_piexls) {
  /*
   return a GArray of break points, including the last break points
   */

  if (reader->base->n_total_req == -1)
    get_num_of_req(reader);

  if (reader->sdata->break_points) {
    if (reader->sdata->break_points->mode == 'v' &&
        (long) reader->sdata->break_points->time_interval == time_interval)
      return reader->sdata->break_points->array;
    else {
      g_array_free(reader->sdata->break_points->array, TRUE);
      free(reader->sdata->break_points);
    }
  }

  gint i;
  gint array_size = (gint) num_of_piexls;
  if (array_size == -1)
    array_size = (gint) ceil((double) reader->base->n_total_req / time_interval + 1);
  else
    time_interval = (gint) ceil((double) reader->base->n_total_req / num_of_piexls + 1);
  //    array_size ++ ;

  GArray *break_points = g_array_new(FALSE, FALSE, sizeof(guint64));
  gint64 value = 0;
  i = 0;
  while (value <= reader->base->n_total_req) {
    g_array_append_val(break_points, value);
    i += 1;
    value = i * time_interval;
  }
  if (value != reader->base->n_total_req)
    g_array_append_val(break_points, reader->base->n_total_req);


  if (break_points->len > 10000) {
    WARNING("number of pixels in one dimension is larger than 10000, "
            "exact size: %d, it may take a very long time, if you didn't "
            "intend to do it, please try with a larger time stamp\n",
            break_points->len);
  } else if (break_points->len < 20) {
    WARNING("number of pixels in one dimension is smaller than 20, "
            "exact size: %d, each pixel will be very large, if you didn't "
            "intend to do this, please try with a smaller time stamp\n",
            break_points->len);
  }

  struct break_point *bp = g_new(struct break_point, 1);
  bp->mode = 'v';
  bp->time_interval = time_interval;
  bp->array = break_points;
  reader->sdata->break_points = bp;

  reset_reader(reader);
  return break_points;
}


GArray *get_bp_rtime(reader_t *reader,
                     gint64 time_interval,
                     gint64 num_of_piexls) {
  /*
   currently this only works for vscsi reader !!!
   return a GArray of break points, including the last break points
   */
  if (reader->base->trace_type == PLAIN_TXT_TRACE) {
    ERROR("get_bp_rtime currently don't support plain reader, program exit\n");
    abort();
  }
  if (reader->base->trace_type == CSV_TRACE) {
    csv_params_t *params = reader->reader_params;
    if (params->real_time_column == -1 || params->real_time_column == 0) {
      ERROR("get_bp_rtime needs you to provide "
            "real_time_column parameter for csv reader\n");
      exit(1);
    }
  }
  if (reader->base->trace_type == BIN_TRACE) {
    binary_params_t *params = reader->reader_params;
    if (params->real_time_pos == 0) {
      ERROR("get_bp_rtime needs you to provide "
            "real_time parameter for binary reader\n");
      exit(1);
    }
  }


  if (reader->base->n_total_req == -1)
    get_num_of_req(reader);


  if (reader->sdata->break_points) {
    if (reader->sdata->break_points->mode == 'r' &&
        (long) reader->sdata->break_points->time_interval == time_interval) {
      return reader->sdata->break_points->array;
    } else {
      g_array_free(reader->sdata->break_points->array, TRUE);
      free(reader->sdata->break_points);
    }
  }

  guint64 previous_time = 0;
  GArray *break_points = g_array_new(FALSE, FALSE, sizeof(guint64));

  // create request struct and initialization
  request_t *req = new_request(reader->base->obj_id_type);

  guint64 num = 0;

  reset_reader(reader);
  read_one_req(reader, req);
  previous_time = req->real_time;
  g_array_append_val(break_points, num);


  if (num_of_piexls != -1 && time_interval == -1) {
    reader_set_read_pos(reader, 1);
    read_one_req_above(reader, req);
    time_interval = (gint64) ceil((double) (req->real_time - previous_time) / num_of_piexls + 1);
    reader_set_read_pos(reader, 0);
    read_one_req(reader, req);
  }


  while (req->valid) {
    if (req->real_time - previous_time > (guint64) time_interval) {
      g_array_append_val(break_points, num);
      previous_time = req->real_time;
    }
    read_one_req(reader, req);
    num++;
  }
  if ((gint64) g_array_index(break_points, guint64, break_points->len - 1) != reader->base->n_total_req)
    g_array_append_val(break_points, reader->base->n_total_req);


  if (break_points->len > 10000) {
    WARNING("number of pixels in one dimension is larger than 10000, "
            "exact size: %d, it may take a very long time, if you didn't "
            "intend to do it, please try with a larger time stamp\n",
            break_points->len);
  } else if (break_points->len < 20) {
    WARNING("number of pixels in one dimension is smaller than 20, "
            "exact size: %d, each pixel will be very large, if you didn't "
            "intend to do this, please try with a smaller time stamp\n",
            break_points->len);
  }

  struct break_point *bp = g_new(struct break_point, 1);
  bp->mode = 'r';
  bp->time_interval = time_interval;
  bp->array = break_points;
  reader->sdata->break_points = bp;

  // clean up
  free_request(req);
  reset_reader(reader);
  return break_points;
}


#ifdef __reqlusplus
}
#endif
