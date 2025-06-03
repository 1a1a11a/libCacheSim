#ifndef INCLUDE_CONVERSION_H
#define INCLUDE_CONVERSION_H

#if defined(__cplusplus)
extern "C" {
#endif

#define ptr_t_ void*
#define cptr_t_ const void*

// platform-depedent
#define ptr_size_t_ size_t
#define cptr_size_t_ size_t

// Convert int to pointer for passing to various data structures
#define int_to_ptr(i) (ptr_t_)((ptr_size_t_)i)
#define uint_to_ptr(i) (ptr_t_)((ptr_size_t_)i)
#define int_to_cptr(i) (cptr_t_)((ptr_size_t_)i)
#define uint_to_cptr(i) (cptr_t_)((ptr_size_t_)i)

// Convert pointer to int for extracting from passed parameters
// Take care of int cutoff when pointer is larger than INT_MAX
#define ptr_to_int(p) (int)((unsigned int)((ptr_size_t_)p))
#define ptr_to_uint(p) (unsigned int)((ptr_size_t_)p)
#define ptr_to_long(p) (signed long)((ptr_size_t_)p)
#define ptr_to_ulong(p) (unsigned long)((ptr_size_t_)p)

#if defined(__linux__)
/* platform dependent definitions goes here*/
#elif defined(__APPLE__)
/* platform dependent definitions goes here*/
#else
#error "unsupported operating system"
#endif  // __linux__

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // INCLUDE_CONVERSION_H