#include "dbg_helper.h"

#include <stdlib.h>
#include <stdio.h>


/*
   this function can be used for debug and log
 */
void __dbg_print(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); 
    va_end(ap);

    fflush(stderr);
}


