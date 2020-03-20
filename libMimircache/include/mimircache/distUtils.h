//
// Created by Juncheng Yang on 11/24/19.
//

#ifndef MIMIRCACHE_DISTUTILS_H
#define MIMIRCACHE_DISTUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "reader.h"
#include "csvReader.h"
#include "utils.h"
#include "const.h"
#include "splay.h"



/***********************************************************
 * get the reuse distance (stack distance) since last access,
 * it returns an array of n_req, where the nth element is the
 * reuse distance (number of unique obj) since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_dist(reader_t *reader);

/***********************************************************
 * get the future reuse distance (stack distance) since now,
 * it returns an array of n_req, where the nth element is the
 * future reuse distance (number of unique obj) from now to next access for this obj
 * @param reader
 * @return
 */
gint64 *get_future_reuse_dist(reader_t *reader);


/***********************************************************
 * get the resue time since last access,
 * it returns an array of n_req, where the nth element is the
 * time since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_time(reader_t *reader);

/***********************************************************
 * get the distance since last access,
 * it returns an array of n_req, where the nth element is the
 * distance (number of obj) since last access for this object
 * @param reader
 * @return
 */
gint64 *get_last_access_dist(reader_t *reader);

/***********************************************************
 * get the dist to next access,
 * it returns an array of n_req, where the nth element is the
 * distance (number of obj) since now to next access for this obj
 * note: it is better to rewrite this function to not use read_one_req_above, which has high overhead
 * @param reader
 * @return
 */
gint64 *get_next_access_dist(reader_t *reader);


/***********************************************************
 * NOT WORKING/IMPLETMENTED
 * get the reuse byte (reuse distance in bytes) since last access,
 * it returns an array of n_req, where the nth element is the
 * reuse byte (number of unique obj) since last access for this obj
 * @param reader
 * @return
 */
gint64 *get_reuse_byte(reader_t *reader);

gint64 *get_last_access_byte(reader_t *reader);


/***********************************************************
 *   save the distance array to file to avoid future computation
 *
 * @param reader            the reader for data
 * @param dist_array        distance array to save into file
 * @param path              the output file path
 * @param dist_type         distance type
 * @return
 */
void save_dist(reader_t *const reader, gint64 *dist_array, const char *const path, dist_t dist_type);


/***********************************************************
 *      this function is used for loading distance from the input file, which is pre-computed
 *
 * @param reader                the reader for data
 * @param path                  the input file path
 * @param dist_type             type of distance
 * @return                      distance array in gint64 array
 */
gint64 *load_dist(reader_t *const reader, const char *const path, dist_t dist_type);


#ifdef __cplusplus
}
#endif

#endif //MIMIRCACHE_DISTUTILS_H
