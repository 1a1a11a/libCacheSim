//
// Created by Juncheng Yang on 11/24/19.
//

#ifndef libCacheSim_DISTUTILS_H
#define libCacheSim_DISTUTILS_H

#include "const.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DIST_SINCE_LAST_ACCESS,
  DIST_SINCE_FIRST_ACCESS,
  STACK_DIST,
  FUTURE_STACK_DIST,
} dist_type_e;

static char *g_dist_type_name[] = {
    "DIST_SINCE_LAST_ACCESS",
    "DIST_SINCE_FIRST_ACCESS",
    "STACK_DIST",
    "FUTURE_STACK_DIST",
};

/***********************************************************
 * get the stack distance (number of uniq objects) since last access or till
 * next request,
 *
 * @param reader
 * @param dist_type STACK_DIST or FUTURE_STACK_DIST
 *
 * @return an array of int32_t with size of n_req
 */
int32_t *get_stack_dist(reader_t *reader, const dist_type_e dist_type,
                        int64_t *array_size);

/***********************************************************
 * get the distance (the num of requests) since last/first access

 * @param reader
 * @param dist_type DIST_SINCE_LAST_ACCESS or DIST_SINCE_FIRST_ACCESS
 *
 * @return an array of int32_t with size of n_req
 */
int32_t *get_access_dist(reader_t *reader, const dist_type_e dist_type,
                         int64_t *array_size);

/***********************************************************
 * save the distance array to file to avoid future computation
 *
 * @param reader            the reader for data
 * @param dist_array        distance array to save into file
 * @param path              the output file path
 * @param dist_type         distance type
 * @return
 */
void save_dist(reader_t *const reader, const int32_t *dist_array,
               const int64_t array_size, const char *const ofilepath,
               const dist_type_e dist_type);

/***********************************************************
 * save the distance array to file to avoid future computation,
 * this function is similar to save_dist, but it uses the text format
 */
void save_dist_txt(reader_t *const reader, const int32_t *dist_array,
               const int64_t array_size, const char *const ofilepath,
               const dist_type_e dist_type);

/***********************************************************
 * this function is used for loading distance from the input file
 *
 * @param reader                the reader for data
 * @param dist_type             type of distance
 * @return                      distance array in int32_t array
 */
int32_t *load_dist(reader_t *const reader, const char *const ifilepath,
                   int64_t *array_size);

void save_dist_as_cnt_txt(reader_t *const reader, const int32_t *dist_array,
                      const int64_t array_size, const char *const ofilepath,
                      const dist_type_e dist_type);

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_DISTUTILS_H
