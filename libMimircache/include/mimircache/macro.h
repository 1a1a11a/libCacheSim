





#ifdef __cplusplus
extern "C" {
#endif


#define UNUSED(x)       (void)(x)
#define UNUSED_VAR     __attribute__ ((unused))
#define SUPPRESS_FUNCTION_NO_USE_WARNING(f) (void)f
#define ARRAY_LENGTH(A) (sizeof(A) / sizeof(A[0]))

/* Compare strings */
#define STRCMP(A, o, B) (strcmp((A), (B)) o 0)

/* Compare memory */
#define MEMCMP(A, o, B) (memcmp((A), (B)) o 0)

#define BIT(x) (1 << (x))

#define FILL(instance, field, value)    \
do {                                    \
    instance.field = value;             \
    instance.has_##field = 1;           \
} while(0)



#define PASTE2(a, b) a##b
#define PASTE(a, b) PASTE2(a, b)
//#define CUCKOO(a) PASTE(CUCKOO_TABLE_NAME, a)
  


#ifdef __cplusplus
}
#endif
