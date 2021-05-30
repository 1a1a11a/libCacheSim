
#include "../include/libCacheSim/admissionAlgo.h"
#include "../utils/include/mymath.h"


#ifdef __cplusplus
extern "C" {
#endif

struct size_admissioner {
  uint64_t size_threshold;
};


bool size_admit(void *size_admissioner, request_t *req) {
  struct size_admissioner *a = size_admissioner;
  if (req->obj_size < a->size_threshold) {
    return true;
  }

  return false;
}

void *create_size_admissioner(uint64_t size_threshold) {
  if (size_threshold <= 1) {
    ERROR("size admissioner size error get %"PRIu64
            "(should be 2 - inf)\n", size_threshold);
    abort();
  }

  struct size_admissioner *s = my_malloc(struct size_admissioner);
  memset(s, 0, sizeof(struct size_admissioner));
  s->size_threshold = size_threshold;

  return s;
}

void free_size_admissioner(void *s) {
  my_free(sizeof(struct size_admissioner), s);
}

#ifdef __cplusplus
}
#endif
