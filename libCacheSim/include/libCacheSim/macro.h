

#ifndef libCacheSim_MACRO_H
#define libCacheSim_MACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config.h"
#include <assert.h>

#define UNUSED(x) (void) (x)
#define UNUSED_PARAM __attribute__((unused))// gcc only
#define SUPPRESS_FUNCTION_NO_USE_WARNING(f) (void) f
#define ARRAY_LENGTH(A) (sizeof(A) / sizeof(A[0]))

#ifdef _MSC_VER
#define forceinline __forceinline
#elif defined(__GNUC__)
#define forceinline inline __attribute__((__always_inline__))
#elif defined(__CLANG__)
#if __has_attribute(__always_inline__)
#define forceinline inline __attribute__((__always_inline__))
#else
#define forceinline inline
#endif
#else
#define forceinline inline
#endif

#ifndef __GNUC__
#define __attribute__(x) /*NOTHING*/
#endif

#define BIT(x) (1 << (x))
#define GETNAME(var) #var
#define OFFSETOF(type, element) ((size_t) & (((type *) 0)->element))
#define OFFSETOF2(t, d) __builtin_offsetof(t, d)

#define PASTE(a, b) a##b

#define U(x) ((unsigned) (x))
#define UL(x) ((unsigned long) (x))
#define ULL(x) ((unsigned long long) (x))
#define L(x) ((long) (x))
#define LL(x) ((long long) (x))

#define XSTR(x) STR(x)
#define STR(x) #x

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#undef ABS
#define ABS(a) (((a) < 0) ? -(a) : (a))

#define MAX2(a, b) MAX(a, b)
#define MIN2(a, b) MIN(a, b)

#define MAX3(a, b, c) \
  ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define MIN3(a, b, c) \
  ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

#define SWAP(a, b)            \
  do {                        \
    __typeof__(a) _tmp = (a); \
    (a) = (b);                \
    (b) = _tmp;               \
  } while (0)

#define CHECK_CONDITION(a, op, b, FMT, ...)  \
  do {                                       \
    if ((a) op(b)) {                         \
      printf("%s: %d ", __FILE__, __LINE__); \
      printf(FMT, ##__VA_ARGS__);            \
      fflush(stdout);                        \
      abort();                               \
    }                                        \
  } while (0)

#define ASSERT_NOT_NULL(x, FMT, ...) \
  CHECK_CONDITION(x, ==, NULL, FMT, ##__VA_ARGS__)

#define ASSERT_EQUAL(a, b, FMT, ...) \
  CHECK_CONDITION(a, !=, b, FMT, ##__VA_ARGS__)
#define ASSERT_TRUE(x, FMT, ...) \
  CHECK_CONDITION(x, !=, true, FMT, ##__VA_ARGS__)
#define ASSERT_ZERO(x, FMT, ...) CHECK_CONDITION(a, !=, 0, FMT, ##__VA_ARGS__)

#if LOGLEVEL < INFO_LEVEL
#define DEBUG_ASSERT(x)        \
  do {                         \
      assert(x);               \
  } while (0)
#else
#define DEBUG_ASSERT(x)
#endif

//#pragma message "current LOGLEVEL: " XSTR(LOGLEVEL)

#if LOGLEVEL > DEBUG_LEVEL
#define THIS_IS_DEBUG_FUNC return
#else
#define THIS_IS_DEBUG_FUNC
#endif

#if LOGLEVEL > VERBOSE_LEVEL
#define THIS_IS_DEBUG2_FUNC return
#else
#define THIS_IS_DEBUG2_FUNC
#endif

#if LOGLEVEL > VVERBOSE_LEVEL
#define THIS_IS_DEBUG3_FUNC return
#else
#define THIS_IS_DEBUG3_FUNC
#endif


/*  count the number of one’s(set bits) in an integer */
// _builtin_popcount(x) 
// _builtin_popcountl(x) 
// _builtin_popcountll(x) 

/* check the parity of a number
   returns true(1) if the number has odd parity  */
// _builtin_parity(x) 


/* count the number of leading zeros of the integer */
// __builtin_clz(x) 

/* count the number of trailing zeros, __builtin_ctz(16) == 4 */
// __builtin_ctz(x)

// #include <strings.h>
// ffs, ffsl, ffsll - find first bit (least significant) set in a word 
// int ffs(int i);
// int ffsl(long int i);
// int ffsll(long long int i);



#define find_max(array, n_elem, max_elem_ptr, max_elem_idx_ptr) \
  do {                                                          \
    *max_elem_idx_ptr = 0;                                      \
    for (uint64_t i = 0; i < (uint64_t) n_elem; i++)            \
      if (array[i] > array[*max_elem_idx_ptr])                  \
        *max_elem_idx_ptr = i;                                  \
    *max_elem_ptr = array[*max_elem_idx_ptr];                   \
  } while (0)

#define find_min(array, n_elem, min_elem_ptr, min_elem_idx_ptr) \
  do {                                                          \
    *min_elem_idx_ptr = 0;                                      \
    for (uint64_t i = 0; i < (uint64_t) n_elem; i++)            \
      if (array[i] < array[*min_elem_idx_ptr])                  \
        *min_elem_idx_ptr = i;                                  \
    *min_elem_ptr = array[*min_elem_idx_ptr];                   \
  } while (0)

#define FILL(instance, field, value) \
  do {                               \
    instance.field = value;          \
    instance.has_##field = 1;        \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif
