//
// Created by Juncheng Yang on 4/20/20.
//

#include "include/readerStat.h"

#ifdef __cplusplus
extern "C"
{
#endif


guint64 get_num_of_obj(reader_t *reader) {
    GHashTable* ht = create_hash_table(reader, NULL, NULL, g_free, NULL);
    request_t *req = new_request(reader->base->obj_id_type);
    read_one_req(reader, req);
    while (req->valid){

      read_one_req(reader, req);
    }

    guint64 num_of_obj = g_hash_table_size(ht);

}


#ifdef __cplusplus
}
#endif

