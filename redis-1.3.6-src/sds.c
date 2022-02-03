/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define SDS_ABORT_ON_OOM

#include "sds.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "zmalloc.h"

static void sdsOomAbort(void) {
    fprintf(stderr,"SDS: Out Of Memory (SDS_ABORT_ON_OOM defined)\n");
    abort();
}

/* 
    新建一个sds结构体
    const void *init --> 传入字符数组 
    initlen --> 传入字符数组长度
 */
sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh;

    // 结构体长度 + 传入字符数组长度 + 1（'\0' 加上终止符）
    sh = zmalloc(sizeof(struct sdshdr)+initlen+1);

    // 处理分配失败情况
#ifdef SDS_ABORT_ON_OOM
    if (sh == NULL) sdsOomAbort();
#else
    if (sh == NULL) return NULL;
#endif

    sh->len = initlen;
    sh->free = 0;
    if (initlen) {
        // 拷贝内存，不包含终止符'\0'
        if (init) memcpy(sh->buf, init, initlen);
        else memset(sh->buf,0,initlen);     // 有长度没有字符数组，用 0 初始化
    }
    sh->buf[initlen] = '\0';    // 加上终止符 '\0'
    
    return (char*)sh->buf;      //返回字符数组地址
}

// 初始化字符串 返回'\0'
sds sdsempty(void) {
    return sdsnewlen("",0);
}

// 传入一个字符数组，返回 sds->buf 的地址
sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

// 构造一个指向 sdshdr 的结构体指针，直接读取 len
size_t sdslen(const sds s) {
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    return sh->len;
}

// 拷贝构造一个 sdshdr
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s));
}

//释放结构体
void sdsfree(sds s) {
    if (s == NULL) return;
    // sds --> char * --> sdsher->buf
    // 找到 sdshdr 结构体开头的地址
    zfree(s-sizeof(struct sdshdr));
}

// 返回可用的剩余空间
size_t sdsavail(sds s) {
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    return sh->free;
}

// 更新sds长度
void sdsupdatelen(sds s) {    
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    int reallen = strlen(s);
    sh->free += (sh->len-reallen);  // 更新剩余空间
    sh->len = reallen;              
}

//传入一个 sds(char *) 和 要增加的长度
static sds sdsMakeRoomFor(sds s, size_t addlen) {
    struct sdshdr *sh, *newsh;
    size_t free = sdsavail(s);  // 获取 sds 剩余大小 (s->free)
    size_t len, newlen;

    if (free >= addlen) return s;   // 剩余空间足够，直接返回 s
    // 获取 sds 长度
    len = sdslen(s);
    // sh 指向传入的 s
    sh = (void*) (s-(sizeof(struct sdshdr)));
    // 空间扩大为 2 倍
    newlen = (len+addlen)*2;
    // 同一个指针，申请更大的内存空间 newsh 和 sh 的地址相同
    newsh = zrealloc(sh, sizeof(struct sdshdr)+newlen+1);

// out of memory 的情况
#ifdef SDS_ABORT_ON_OOM
    if (newsh == NULL) sdsOomAbort();
#else
    if (newsh == NULL) return NULL;
#endif

    newsh->free = newlen - len;
    return newsh->buf;
}

// 在原有 sds 后面，添加内容 t， 长度为 len
sds sdscatlen(sds s, void *t, size_t len) {
    struct sdshdr *sh;
    size_t curlen = sdslen(s);

    s = sdsMakeRoomFor(s,len);
    if (s == NULL) return NULL;
    sh = (void*) (s-(sizeof(struct sdshdr)));
    memcpy(s+curlen, t, len);   // 内存拷贝
    sh->len = curlen+len;
    sh->free = sh->free-len;
    s[curlen+len] = '\0';
    return s;
}

// 使用strlen() 调用sdscatlen()
sds sdscat(sds s, char *t) {
    return sdscatlen(s, t, strlen(t));
}

// 拷贝字符串，覆盖原有的 s
sds sdscpylen(sds s, char *t, size_t len) {
    // 获取 sds 对应结构体的地址
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    // 总的空间大小
    size_t totlen = sh->free+sh->len;

    if (totlen < len) {
        s = sdsMakeRoomFor(s, len - sh->len);
        if (s == NULL) return NULL;
        sh = (void*) (s-(sizeof(struct sdshdr)));
        // 申请更大空间，更新 len 和 free
        totlen = sh->free+sh->len;
    }

    memcpy(s, t, len);
    s[len] = '\0';
    sh->len = len;
    sh->free = totlen-len;
    return s;
}

// 使用 strlen()，调用sdscpylen()
sds sdscpy(sds s, char *t) {
    return sdscpylen(s, t, strlen(t));
}

// 
sds sdscatprintf(sds s, const char *fmt, ...) {
    // 入参 ... 的内容保存在 ap 中
    // 根据 fmt 格式化
    va_list ap;
    char *buf, *t;
    size_t buflen = 16;

    while(1) {
        buf = zmalloc(buflen);

#ifdef SDS_ABORT_ON_OOM
        if (buf == NULL) sdsOomAbort();
#else
        if (buf == NULL) return NULL;
#endif
        // 16字节 buf[14]
        buf[buflen-2] = '\0';
        // 获取可变参数链列表
        va_start(ap, fmt);
        // buf 保存要打印的字符串，fmt 为打印格式，ap 是要打印的列表
        vsnprintf(buf, buflen, fmt, ap);
        va_end(ap);     // 释放资源
        // 如果 ‘\0' 被覆盖说明显示长度超了，需要扩充长度
        if (buf[buflen-2] != '\0') {
            zfree(buf);
            buflen *= 2;
            continue;
        }
        break;
    }
    // 传入的 sds 加上 buf 的内容
    t = sdscat(s, buf);
    zfree(buf);     // 释放资源
    return t;
}

// 移除前后多余的字符（空格）
// cset 要移除的字符
sds sdstrim(sds s, const char *cset) {
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;         // s 是指向 sh 的 buf 成员的首地址
    ep = end = s + sdslen(s) - 1;   // 不考虑最后的 '\0' 
    // strchr() 在字符串中搜索第一次出现某个字符的位置
    while(sp <= end && strchr(cset, *sp)) sp++;     // 从头搜索
    while(ep > start && strchr(cset, *ep)) ep--;    // 从尾部搜索
    // 结束时，sp 和 ep 指向第一个和最后一个有效字符
    // len 为有效字符串长度
    len = (sp > ep) ? 0 : ((ep-sp)+1);
    // 移动有效字符串
    if (sh->buf != sp) memmove(sh->buf, sp, len);
    sh->buf[len] = '\0';
    // 更新 free 和 len
    sh->free = sh->free+(sh->len-len);
    sh->len = len;
    return s;
}

// 使用 [start, end] 进行范围移动，覆盖原始字符串
sds sdsrange(sds s, long start, long end) {
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    size_t newlen, len = sdslen(s);

    if (len == 0) return s;
    if (start < 0) {
        // start < 0，表示从负数位置开始算（从后往前）
        start = len+start;
        if (start < 0) start = 0;   // 超过长度，直接设为0
    }
    if (end < 0) {
        end = len+end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end-start)+1;
    if (newlen != 0) {
        // 判断 start 和 end 是否在正常长度范围
        if (start >= (signed)len) start = len-1;
        if (end >= (signed)len) end = len-1;
        newlen = (start > end) ? 0 : (end-start)+1;
    } else {
        start = 0;
    }
    if (start != 0) memmove(sh->buf, sh->buf+start, newlen);
    sh->buf[newlen] = 0;    // == '\0'
    sh->free = sh->free+(sh->len-newlen);
    sh->len = newlen;
    return s;
}

// 转为小写
void sdstolower(sds s) {
    int len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

// 转为大写
void sdstoupper(sds s) {
    int len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

// 比较两个 sds 字符串
int sdscmp(sds s1, sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1,s2,minlen);
    if (cmp == 0) return l1-l2;
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
// 分割字符串 
/*  s:      待分割字符串
    len:    s 的长度  
    sep:    分割标识字符串
    seplen: sep 的长度
    count:  结果的数量（指针）
*/
sds *sdssplitlen(char *s, int len, char *sep, int seplen, int *count) {
    int elements = 0, slots = 5, start = 0, j;
    // token --> 指向 sds 的结果数组
    sds *tokens = zmalloc(sizeof(sds)* slots);
#ifdef SDS_ABORT_ON_OOM
    if (tokens == NULL) sdsOomAbort();
#endif
    if (seplen < 1 || len < 0 || tokens == NULL) return NULL;
    if (len == 0) {
        *count = 0;
        return tokens;
    }
    // 根据 sep长度 设置遍历终止位置
    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            sds *newtokens;
            slots *= 2;
            newtokens = zrealloc(tokens,sizeof(sds)*slots);
            if (newtokens == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
            }
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0])   // sep 长度为1，直接比较
        || 
        (memcmp(s+j,sep,seplen) == 0)) {        // sep 长度大于1，使用 memcmp
            tokens[elements] = sdsnewlen(s+start,j-start);
            if (tokens[elements] == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
            }
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s+start,len-start);
    if (tokens[elements] == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
    }
    elements++; // token 的个数 = index + 1
    *count = elements;
    return tokens;

#ifndef SDS_ABORT_ON_OOM
cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        zfree(tokens);
        return NULL;
    }
#endif
}
