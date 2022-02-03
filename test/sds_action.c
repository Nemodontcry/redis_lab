#include <stdio.h>
#include "../redis-1.3.6-src/zmalloc.c"
#include "../redis-1.3.6-src/sds.c"



int main(int argc, char** argv){
    sds s;
    s = sdsnew("hello");
    printf("sdslen(s) = %zu\n", sdslen(s));
    printf("sdsavail(s) = %zu\n", sdsavail(s));
    printf("s = %s\n", s);

    sds s1;
    // s1 = sdsnew("world");
    sdsMakeRoomFor(s, 10);
    printf("sdslen(s) = %zu\n", sdslen(s));
    printf("sdsavail(s) = %zu\n", sdsavail(s));
    printf("s = %s\n", s);

    sdscat(s, ",world");
    printf("sdslen(s) = %zu\n", sdslen(s));
    printf("sdsavail(s) = %zu\n", sdsavail(s));
    printf("s = %s\n", s); 
    
    return 0;
}

