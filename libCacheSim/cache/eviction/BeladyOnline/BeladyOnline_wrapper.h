#ifndef BELADYONLINE_WRAPPER_H
#define BELADYONLINE_WRAPPER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BeladyOnline BeladyOnline_t;

BeladyOnline_t* BeladyOnline_new(int64_t cache_size);
bool _BeladyOnline_get(BeladyOnline_t* obj, int64_t obj_id, int64_t size);
void BeladyOnline_delete(BeladyOnline_t* obj);

#ifdef __cplusplus
}
#endif

#endif