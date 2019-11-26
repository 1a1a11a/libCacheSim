//
//  csvReader.h
//  mimircache
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef csvReader_h
#define csvReader_h

#include "reader.h"
#include "logging.h"

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct{
    gint obj_id_field;
    gint op_field;
    gint real_time_field;
    gint size_field;
    gboolean has_header;
    unsigned char delimiter;
    gint traceID_field;
}csvReader_init_params; 


typedef struct{
    gboolean has_header;
    unsigned char delim;
    struct csv_parser *csv_parser;
    
    gint real_time_field;          /* column number begins from 1 */
    gint obj_id_field;
    gint op_field;
    gint size_field;
    gint traceID_field;
    gint current_field_counter;
    
    void* req_pointer;
    gboolean already_got_req;
    gboolean reader_end;

}csv_params_t;


csvReader_init_params* new_csvReader_init_params(gint, gint, gint, gint,
                                                 gboolean, unsigned char, gint);

void csv_setup_Reader(const char *const file_loc,
                      reader_t *const reader,
                      const csvReader_init_params *const init_params);

void csv_read_one_element(reader_t *const,
                          request_t *const);

int csv_go_back_one_line(reader_t*);

guint64 csv_skip_N_elements(reader_t *const reader,
                            const guint64 N);
void csv_reset_reader(reader_t* reader); 
void csv_set_no_eof(reader_t* reader);


#ifdef __cplusplus
}
#endif


#endif /* csvReader_h */
