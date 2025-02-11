//
// Created by Zhenyu Song on 10/31/18.
//

#ifndef WEBCACHESIM_COMMON_H
#define WEBCACHESIM_COMMON_H

// uncomment to enable cache debugging:
#undef CDEBUG
//#define CDEBUG

// util for debug
#ifdef CDEBUG
inline void logMessage(std::string m, uint64_t x, uint64_t y, uint64_t z) {
//    std::cerr << m << "," << x << "," << y  << "," << z << "\n";
    printf("%s, %lu, %lu, %lu\n", m.c_str(), x, y, z);
}
#define LOG(m,x,y,z) logMessage(m,x,y,z)
#else
#define LOG(m,x,y,z)
#endif


#ifdef CDEBUG
//#define DPRINTF(fmt, args...) printf("%s: " fmt, __func__ , ## args)
#define DPRINTF(fmt, args...) printf(fmt, ## args)
#else
#define DPRINTF(fmt, args...) do {} while (0)
#endif

#ifdef CDEBUG
#define DASSERT(expr)						\
    if (!(expr)) {						\
        _printf("Assertion failed! %s, %s, %s, line=%d\n",	\
               #expr, __FILE__, __func__, __LINE__); \
    }
#else
#define DASSERT(expr) do {} while (0)
#endif

#ifdef CDEBUG
#define DASSERT2(expr1, expr2)						\
    if (expr1 != expr2) {						\
        _printf("Assertion failed! %s, %d, %s, %d, %s, %s, line=%d\n",	\
               #expr1, expr1, #expr2, expr2, __FILE__, __func__, __LINE__);	\
    }
#else
#define DASSERT2(expr1, expr2) do {} while (0)
#endif


#endif //WEBCACHESIM_COMMON_H