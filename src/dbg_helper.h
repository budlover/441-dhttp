#ifndef _DBG_HELPER_H
#define _DBG_HELPER_H

#include <stdarg.h>
#include <assert.h>
void __dbg_print(const char *fmt, ...);

#ifdef DEBUG

#define DBG_LEVEL 0

#define dbg_print(level, fmt, arg...)                                       \
    do {                                                                    \
                                                                            \
        if(level >= DBG_LEVEL)                                              \
        {                                                                   \
            __dbg_print(fmt, ##arg);                                         \
/*          fprintf(stderr, "In %s aroud line: %d\n", __FILE__, __LINE__); */ \
        }                                                                   \
    } while(0)

#else

#define dbg_print(fmt, arg...)
#endif

#define ASSERT(x)       \
    do{                 \
        fprintf(stderr, x); \
        assert(0);      \
    }while(0)


#endif
