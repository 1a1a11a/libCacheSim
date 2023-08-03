//
// Created by Juncheng Yang on 2019-06-21.
//

#ifndef CONSISTENT_HASH_H_
#define CONSISTENT_HASH_H_

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define N_VNODE_PER_SERVER 160

typedef struct {
  unsigned int point;  // point on ring
  unsigned int server_id;
} vnode_t;

typedef struct {
  unsigned int n_point;
  unsigned int n_server;
  vnode_t *vnodes;
} ring_t;

// typedef int (*compfn)(const void *, const void *);

/**
 * @brief create a consistent hash ring with n servers
 *
 * @param n_server
 * @param weight null if all servers have the same weight
 * @return ring_t*
 */
ring_t *ch_ring_create_ring(int n_server, double *weight);

/**
 * @brief retrieve the server id from the consistent hash ring
 *
 * @param key
 * @param ring
 * @return vnode_t*
 */
// vnode_t *ch_ring_get_server(const char *const key, const ring_t *const ring);

int ch_ring_get_server(const char *const key, const ring_t *const ring);

/**
 * @brief retrieve the server id from the consistent hash ring
 *
 * @param obj_id
 * @param ring
 * @return int
 */
int ch_ring_get_server_from_uint64(uint64_t obj_id, const ring_t *const ring);

/**
 * @brief retrieve the n consecutive server ids from the consistent hash ring
 *
 * @param key
 * @param ring
 * @param n
 * @param idxs
 */
void ch_ring_get_servers(const char *const key, const ring_t *const ring,
                         const unsigned int n, unsigned int *idxs);

/**
 * @brief retrieve n consecutive available server ids from the consistent hash
 * ring
 *
 * @param key
 * @param ring
 * @param n
 * @param idxs
 * @param server_unavailability
 */
void ch_ring_get_available_servers(const char *const key,
                                   const ring_t *const ring,
                                   const unsigned int n, unsigned int *idxs,
                                   const char *server_unavailability);

/**
 * @brief destroy consistent hash ring
 *
 * @param ring
 */
void ch_ring_destroy_ring(ring_t *ring);

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

#endif  // CONSISTENT_HASH_H_
