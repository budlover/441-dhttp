#include <string.h>

/* 
   convert the num to the bucket index
 */
size_t uint_to_idx(void *key, size_t bucket_size)
{
    return (size_t)key % bucket_size;
}

int cmp_int(const void *int1, const void *int2)
{
    if((long)int1 == (long)int2)
        return 0;

    return -1;
}

/* 
   convert the sting to the bucket index
 */
size_t str_to_idx(void *key, size_t bucket_size)
{
    size_t sum = 0;
    char *c = key;
    while(*c)
    {
        sum += *c;
        c++;
    }
    sum = sum % bucket_size;

    return sum;
}


int cmpstr(const void *str1, const void *str2)
{
    return strcmp(str1, str2);
}
