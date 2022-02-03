#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../redis-1.3.6-src/zmalloc.h"
#include "../redis-1.3.6-src/sds.h"

typedef char *sds;  // sgs == char *

// struct sdshdr {
//     long len;       // 当前字符长度
//     long free;      // 剩余长度
//     char buf[];     // 字符数组  "hello" --> ['h','e','l','l','o','\0']
// };

// 必须在main外初始化
struct sdshdr sh = { 6, 2, {'h','e','l','l','o','\0'}};

// 与系统函数 snprintf() 相同
int my_snprintf(char* s, int size, const char *fmt, ...){
    va_list ap;
    int n = 0;
    va_start(ap, fmt);  // 获取可变参数列表
    n = vsnprintf(s, size, fmt, ap);    // 写入字符串 s
    va_end(ap); //释放资源
    return n;   // 返回写入的字符个数
}


int main(int argc, char** argv){
    char* str1 = argv[1];
    char* str2 = argv[2];
    // const char init_s1[] = {"hello"};
    sds s1 = sdsnew(str1);
    // const char init_s2[] = {"hello world"};
    sds s2 = sdsnew(str2);
    int cmp = sdscmp(s1, s2);

    printf("%d\n", cmp);
    return 0;
}


// int main() {
//     struct sdshdr *s = &sh;
//     printf("sizeof(struct sdshdr): %lu\n", sizeof(struct sdshdr));
//     printf("sizeof(long): %lu\n", sizeof(long));
//     printf("%ld, %p\n", s->len, &(s->len));
//     printf("%ld, %p\n", s->free, &(s->free));
//     printf("%s, %p\n", s->buf, &(s->buf));
    
//     sds sd = s->buf;    // 字符数组的起始地址位置
//     printf("%s\n", sd);
    
//     struct sdshdr *new_sh = (void*) (sd - sizeof(struct sdshdr));
//     printf("得到当前字符长度：%ld，剩余长度：%ld。\n", new_sh->len, new_sh->free);
    
//     char str[1024];
//     snprintf(str, sizeof(str), "%d, %d, %d, %d\n", 5, 6, 7, 8);
//     printf("%s\n", str);

//     return 0;
// }