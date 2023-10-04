//
// Created by Juncheng Yang on 2019-06-21.
//
// modified from libketama
//

#include "include/consistentHash.h"

#include <inttypes.h>
#include <stdbool.h>

#include "include/md5.h"

#ifdef __cplusplus
extern "C" {
#endif

int ch_ring_compare(const void *a, const void *b) {
  vnode_t *node_a = (vnode_t *)a;
  vnode_t *node_b = (vnode_t *)b;
  return (node_a->point < node_b->point)
             ? -1
             : ((node_a->point > node_b->point) ? 1 : 0);
}

void md5_digest(const char *const inString, unsigned char md5pword[16]) {
  md5_state_t md5state;

  md5_init(&md5state);
  md5_append(&md5state, (unsigned char *)inString, (int)strlen(inString));
  md5_finish(&md5state, md5pword);
}

unsigned int ketama_hash(const char *const inString) {
  unsigned char digest[16];
  md5_digest(inString, digest);
  return (unsigned int)((digest[3] << 24) | (digest[2] << 16) |
                        (digest[1] << 8) | digest[0]);
}

ring_t *ch_ring_create_ring(int n_server, double *weight) {
  vnode_t *vnodes =
      (vnode_t *)malloc(sizeof(vnode_t) * n_server * N_VNODE_PER_SERVER);

  ring_t *ring = (ring_t *)malloc(sizeof(ring_t));
  ring->n_server = n_server;
  ring->n_point = n_server * N_VNODE_PER_SERVER;
  ring->vnodes = vnodes;

  int i;
  unsigned int k, cnt = 0;

  for (i = 0; i < n_server; i++) {
    // default all servers have the same weight
    unsigned int ks = N_VNODE_PER_SERVER / 4;
    if (weight != NULL)
      ks = (unsigned int)floorf(weight[i] * (float)n_server *
                                (N_VNODE_PER_SERVER / 4));

    for (k = 0; k < ks; k++) {
      /* 40 hashes, 4 numbers per hash = 160 points per server */
      char ss[30];
      unsigned char digest[16];

      sprintf(ss, "%u-%u", i, k);
      md5_digest(ss, digest);

      /* Use successive 4-bytes from hash as numbers for the points on the
       * circle: */
      int h;
      for (h = 0; h < 4; h++) {
        // printf("%d i %d k %d h %d %d %d\n", cnt, i, k, h, ring->n_server,
        // ring->n_point);
        vnodes[cnt].point = (digest[3 + h * 4] << 24) |
                            (digest[2 + h * 4] << 16) |
                            (digest[1 + h * 4] << 8) | digest[h * 4];

        vnodes[cnt].server_id = i;
        cnt++;
      }
    }
  }

  /* Sorts in ascending order of "point" */
  qsort((void *)vnodes, cnt, sizeof(vnode_t), ch_ring_compare);

  //  for (i=0; i< 200; i++)
  //    printf("%u %u\n", vnodes[i].point, vnodes[i].server_id);
  return ring;
}

int ch_ring_get_vnode_idx(const char *const key, const ring_t *const ring) {
  unsigned int h = ketama_hash(key);
  int highp = ring->n_point;
  vnode_t *vnodes = ring->vnodes;
  int lowp = 0;
  unsigned int midp;
  unsigned int midval, midval1;
  int vnode_idx = -1;

  // divide and conquer array search to find server with next biggest
  // point after what this key hashes to
  while (true) {
    midp = (int)((lowp + highp) / 2);

    if (midp == ring->n_point) {
      vnode_idx = 0;
      return vnode_idx;
    }

    midval = vnodes[midp].point;
    midval1 = midp == 0 ? 0 : vnodes[midp - 1].point;

    if (h <= midval && h > midval1)
      vnode_idx = midp;
    else {
      if (midval < h)
        lowp = midp + 1;
      else
        highp = midp - 1;

      if (lowp > highp) vnode_idx = 0;
    }

    if (vnode_idx != -1) break;
  }
  return vnode_idx;
}

// vnode_t *ch_ring_get_server(const char *const key, const ring_t *const ring)
// {
//   return ring->vnodes + ch_ring_get_vnode_idx(key, ring);
// }

int ch_ring_get_server(const char *const key, const ring_t *const ring) {
  return (ring->vnodes + ch_ring_get_vnode_idx(key, ring))->server_id;
}

int ch_ring_get_server_from_uint64(uint64_t obj_id,
                                     const ring_t *const ring) {
  char key[8]; 
  memcpy(key, (char *) &obj_id, 8);
  key[7] = 0;
  return (ring->vnodes + ch_ring_get_vnode_idx(key, ring))->server_id;
}

/* n: the number of servers that's going to retrieve */
void ch_ring_get_servers(const char *const key, const ring_t *const ring,
                         const unsigned int n, unsigned int *idxs) {
  vnode_t *vnodes = ring->vnodes;
  int start_vnode_idx = ch_ring_get_vnode_idx(key, ring);

  unsigned int i = 0, vnode_pos = 0;
  char chosen_server[ring->n_server];
  memset(chosen_server, 0, sizeof(char) * ring->n_server);
  while (i < n) {
    unsigned int server_id =
        vnodes[(start_vnode_idx + vnode_pos) % (ring->n_point)].server_id;
    if (chosen_server[server_id] == 0) {
      idxs[i] = server_id;
      chosen_server[server_id] = 1;
      i++;
    }
    vnode_pos++;
    if (vnode_pos > ring->n_point) {
      printf(
          "ERROR: searched all points on the consistent hash ring, but cannot "
          "find enough servers\n");
      abort();
    }
  }
}

void ch_ring_get_available_servers(const char *const key,
                                   const ring_t *const ring,
                                   const unsigned int n, unsigned int *idxs,
                                   const char *server_unavailability) {
  vnode_t *vnodes = ring->vnodes;
  int start_vnode_idx = ch_ring_get_vnode_idx(key, ring);

  unsigned int i = 0, vnode_pos = 0;
  unsigned int server_id;
  char chosen_server[ring->n_server];
  memcpy(chosen_server, server_unavailability, sizeof(char) * ring->n_server);
  while (i < n) {
    server_id =
        vnodes[(start_vnode_idx + vnode_pos) % (ring->n_point)].server_id;
    if (chosen_server[server_id] == 0) {
      idxs[i] = server_id;
      chosen_server[server_id] = 1;
      i++;
      //      printf("start %d - i %d - pos %d - server %d set\n",
      //      start_vnode_idx, i, vnode_pos, server_id);
    }

    vnode_pos++;
    //    printf("start %d - i %d - pos %d - server %d ignore\n",
    //    start_vnode_idx, i, vnode_pos, server_id);
    if (vnode_pos > ring->n_point) {
      printf(
          "ERROR: searched all %u points on the consistent hash ring, but "
          "cannot find %u available servers\n",
          ring->n_point, n);
      for (unsigned int j = 0; j < 10; j++) printf("%d ", chosen_server[j]);
      printf("\n");
      for (unsigned j = 0; j < n; j++) printf("%d, ", idxs[j]);
      printf("\n");
      abort();
    }
  }
}

void ch_ring_destroy_ring(ring_t *ring) {
  free(ring->vnodes);
  free(ring);
}

#ifdef __cplusplus
}
#endif