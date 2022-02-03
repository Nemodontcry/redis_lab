/* Redis CLI (command line interface)
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include "fmacros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

#include "anet.h"
#include "sds.h"
#include "adlist.h"
#include "zmalloc.h"

// 自己的日志
static void ownLog(const char *fmt, ...) {
    va_list ap;
    FILE *fp;

    fp = stdout;
    if (!fp) return;

    va_start(ap, fmt);
    if (1) {
        char buf[64];
        time_t now;

        now = time(NULL);
        strftime(buf,64,"%d %b %H:%M:%S",localtime(&now));
        fprintf(fp,"ownLog [%d] %s ",(int)getpid(),buf);
        vfprintf(fp, fmt, ap);
        fprintf(fp,"\n");
        fflush(fp);
    }
    va_end(ap);
}

static const char* getStrHex(const char *s)
{
    static  char buf[1024];
  while(*s)
    sprintf(buf + strlen(buf), "%02x", (unsigned int) *s++);
  return buf;
}

#define REDIS_CMD_INLINE 1
#define REDIS_CMD_BULK 2
#define REDIS_CMD_MULTIBULK 4

#define REDIS_NOTUSED(V) ((void) V)

// 访问redis的配置信息
static struct config {
    char *hostip;
    int hostport;
    long repeat;
    int dbnum;
    int interactive;
    char *auth;
} config;

struct redisCommand {
    char *name;
    int arity;
    int flags;
};

// 要区分BULK和INLINE是因为INLINE是那种参数不会出现\r\n的
// 而BULK的参数是有可能出现\r\n这种会影响原来协议判断的，例如 value的值 就是BULK的了
// 需要先传value的长度，再传value值
// MULTIBULK 貌似是更复杂的情况

static struct redisCommand cmdTable[] = {
    {"auth",2,REDIS_CMD_INLINE},
    {"get",2,REDIS_CMD_INLINE},
    {"set",3,REDIS_CMD_BULK},
    {"setnx",3,REDIS_CMD_BULK},
    {"append",3,REDIS_CMD_BULK},
    {"substr",4,REDIS_CMD_INLINE},
    {"del",-2,REDIS_CMD_INLINE},
    {"exists",2,REDIS_CMD_INLINE},
    {"incr",2,REDIS_CMD_INLINE},
    {"decr",2,REDIS_CMD_INLINE},
    {"rpush",3,REDIS_CMD_BULK},
    {"lpush",3,REDIS_CMD_BULK},
    {"rpop",2,REDIS_CMD_INLINE},
    {"lpop",2,REDIS_CMD_INLINE},
    {"brpop",-3,REDIS_CMD_INLINE},
    {"blpop",-3,REDIS_CMD_INLINE},
    {"llen",2,REDIS_CMD_INLINE},
    {"lindex",3,REDIS_CMD_INLINE},
    {"lset",4,REDIS_CMD_BULK},
    {"lrange",4,REDIS_CMD_INLINE},
    {"ltrim",4,REDIS_CMD_INLINE},
    {"lrem",4,REDIS_CMD_BULK},
    {"rpoplpush",3,REDIS_CMD_BULK},
    {"sadd",3,REDIS_CMD_BULK},
    {"srem",3,REDIS_CMD_BULK},
    {"smove",4,REDIS_CMD_BULK},
    {"sismember",3,REDIS_CMD_BULK},
    {"scard",2,REDIS_CMD_INLINE},
    {"spop",2,REDIS_CMD_INLINE},
    {"srandmember",2,REDIS_CMD_INLINE},
    {"sinter",-2,REDIS_CMD_INLINE},
    {"sinterstore",-3,REDIS_CMD_INLINE},
    {"sunion",-2,REDIS_CMD_INLINE},
    {"sunionstore",-3,REDIS_CMD_INLINE},
    {"sdiff",-2,REDIS_CMD_INLINE},
    {"sdiffstore",-3,REDIS_CMD_INLINE},
    {"smembers",2,REDIS_CMD_INLINE},
    {"zadd",4,REDIS_CMD_BULK},
    {"zincrby",4,REDIS_CMD_BULK},
    {"zrem",3,REDIS_CMD_BULK},
    {"zremrangebyscore",4,REDIS_CMD_INLINE},
    {"zmerge",-3,REDIS_CMD_INLINE},
    {"zmergeweighed",-4,REDIS_CMD_INLINE},
    {"zrange",-4,REDIS_CMD_INLINE},
    {"zrank",3,REDIS_CMD_BULK},
    {"zrevrank",3,REDIS_CMD_BULK},
    {"zrangebyscore",-4,REDIS_CMD_INLINE},
    {"zcount",4,REDIS_CMD_INLINE},
    {"zrevrange",-4,REDIS_CMD_INLINE},
    {"zcard",2,REDIS_CMD_INLINE},
    {"zscore",3,REDIS_CMD_BULK},
    {"incrby",3,REDIS_CMD_INLINE},
    {"decrby",3,REDIS_CMD_INLINE},
    {"getset",3,REDIS_CMD_BULK},
    {"randomkey",1,REDIS_CMD_INLINE},
    {"select",2,REDIS_CMD_INLINE},
    {"move",3,REDIS_CMD_INLINE},
    {"rename",3,REDIS_CMD_INLINE},
    {"renamenx",3,REDIS_CMD_INLINE},
    {"keys",2,REDIS_CMD_INLINE},
    {"dbsize",1,REDIS_CMD_INLINE},
    {"ping",1,REDIS_CMD_INLINE},
    {"echo",2,REDIS_CMD_BULK},
    {"save",1,REDIS_CMD_INLINE},
    {"bgsave",1,REDIS_CMD_INLINE},
    {"rewriteaof",1,REDIS_CMD_INLINE},
    {"bgrewriteaof",1,REDIS_CMD_INLINE},
    {"shutdown",1,REDIS_CMD_INLINE},
    {"lastsave",1,REDIS_CMD_INLINE},
    {"type",2,REDIS_CMD_INLINE},
    {"flushdb",1,REDIS_CMD_INLINE},
    {"flushall",1,REDIS_CMD_INLINE},
    {"sort",-2,REDIS_CMD_INLINE},
    {"info",1,REDIS_CMD_INLINE},
    {"mget",-2,REDIS_CMD_INLINE},
    {"expire",3,REDIS_CMD_INLINE},
    {"expireat",3,REDIS_CMD_INLINE},
    {"ttl",2,REDIS_CMD_INLINE},
    {"slaveof",3,REDIS_CMD_INLINE},
    {"debug",-2,REDIS_CMD_INLINE},
    {"mset",-3,REDIS_CMD_MULTIBULK},
    {"msetnx",-3,REDIS_CMD_MULTIBULK},
    {"monitor",1,REDIS_CMD_INLINE},
    {"multi",1,REDIS_CMD_INLINE},
    {"exec",1,REDIS_CMD_INLINE},
    {"discard",1,REDIS_CMD_INLINE},
    {"hset",4,REDIS_CMD_MULTIBULK},
    {"hget",3,REDIS_CMD_BULK},
    {"hdel",3,REDIS_CMD_BULK},
    {"hlen",2,REDIS_CMD_INLINE},
    {"hkeys",2,REDIS_CMD_INLINE},
    {"hvals",2,REDIS_CMD_INLINE},
    {"hgetall",2,REDIS_CMD_INLINE},
    {"hexists",3,REDIS_CMD_BULK},
    {NULL,0,0}
};

static int cliReadReply(int fd);
static void usage();

static struct redisCommand *lookupCommand(char *name) {
    int j = 0;
    while(cmdTable[j].name != NULL) {
        // 遍历 cmdTable 查找命令
        // strcasecmp 忽略大小写
        if (!strcasecmp(name,cmdTable[j].name)) return &cmdTable[j];
        j++;
    }
    return NULL;
}

static int cliConnect(void) {
    char err[ANET_ERR_LEN];
    static int fd = ANET_ERR;

    if (fd == ANET_ERR) {
        // fd --> socket
        fd = anetTcpConnect(err,config.hostip,config.hostport);
        if (fd == ANET_ERR) {
            fprintf(stderr, "Could not connect to Redis at %s:%d: %s", config.hostip, config.hostport, err);
            return -1;
        }
        anetTcpNoDelay(NULL,fd);
    }
    return fd;
}

static sds cliReadLine(int fd) {
    sds line = sdsempty();

    while(1) {
        char c;
        ssize_t ret;

        ret = read(fd,&c,1);
        if (ret == -1) {
            sdsfree(line);
            return NULL;
        } else if ((ret == 0) || (c == '\n')) {
            break;
        } else {
            line = sdscatlen(line,&c,1);
        }
    }
    return sdstrim(line,"\r\n");
}

static int cliReadSingleLineReply(int fd, int quiet) {
    sds reply = cliReadLine(fd);

    if (reply == NULL) return 1;
    if (!quiet) 
        printf("%s\n", reply);
    sdsfree(reply);
    return 0;
}

static int cliReadBulkReply(int fd) {
    sds replylen = cliReadLine(fd);
    char *reply, crlf[2];
    int bulklen;

    if (replylen == NULL) return 1;
    
    bulklen = atoi(replylen);   // 字符串转为int
    if (bulklen == -1) {
        sdsfree(replylen);
        printf("(nil)\n");
        return 0;
    }
    reply = zmalloc(bulklen);
    anetRead(fd,reply,bulklen);
    anetRead(fd,crlf,2);
    if (bulklen && fwrite(reply,bulklen,1,stdout) == 0) {
        zfree(reply);
        return 1;
    }
    if (isatty(fileno(stdout)) && reply[bulklen-1] != '\n')
        printf("\n");
    zfree(reply);
    return 0;
}

static int cliReadMultiBulkReply(int fd) {
    sds replylen = cliReadLine(fd);
    int elements, c = 1;

    if (replylen == NULL) return 1;
    elements = atoi(replylen);
    if (elements == -1) {
        sdsfree(replylen);
        printf("(nil)\n");
        return 0;
    }
    if (elements == 0) {
        printf("(empty list or set)\n");
    }
    while(elements--) {
        printf("%d. ", c);
        if (cliReadReply(fd)) return 1;
        c++;
    }
    return 0;
}

static int cliReadReply(int fd) {
    char type;

    if (anetRead(fd, &type, 1) <= 0) exit(1);
    switch(type) {
    case '-':               // 错误信息
        printf("(error) ");
        cliReadSingleLineReply(fd,0);
        return 1;
    case '+':               // 正确结果
        return cliReadSingleLineReply(fd,0);
    case ':':               // 整形
        printf("(integer) ");
        return cliReadSingleLineReply(fd,0);
    case '$':               // 参数响应
        return cliReadBulkReply(fd);
    case '*':               // 多参数响应
        return cliReadMultiBulkReply(fd);
    default:
        printf("protocol error, got '%c' as reply type byte\n", type);
        return 1;
    }
}

static int selectDb(int fd) {
    int retval;
    sds cmd;
    char type;

    // 默认数据库 0
    if (config.dbnum == 0)
        return 0;

    cmd = sdsempty();
    cmd = sdscatprintf(cmd,"SELECT %d\r\n",config.dbnum);

    // 向 redis 服务发送字符串
    anetWrite(fd,cmd,sdslen(cmd));
    // 读取 server 返回的结果
    anetRead(fd,&type,1);
    if (type <= 0 || type != '+') return 1;
    // '+' 
    retval = cliReadSingleLineReply(fd,1);
    if (retval) {
        return retval;
    }
    return 0;
}

static int cliSendCommand(int argc, char **argv) {
    // lookupCommand() 查看命令是否符合要求
    // argv[0] 第一个参数 即 命令参数
    struct redisCommand *rc = lookupCommand(argv[0]);
    int fd, j, retval = 0;
    int read_forever = 0;
    sds cmd;

    // 如果命令不存在
    if (!rc) {
        fprintf(stderr,"Unknown command '%s'\n",argv[0]);
        return 1;
    }

    // 判断参数数量是否正确
    if ((rc->arity > 0 && argc != rc->arity) ||
        (rc->arity < 0 && argc < -rc->arity)) {
            fprintf(stderr,"Wrong number of arguments for '%s'\n",rc->name);
            return 1;
    }

    // 输入 monitor 进入监听模式
    if (!strcasecmp(rc->name,"monitor")) read_forever = 1;
    // 建立与服务端的连接，得到套接字 redis server
    if ((fd = cliConnect()) == -1) return 1;


    /* Select db number */
    // 选择数据库
    retval = selectDb(fd);
    // retval 为1 有错误， 为0，无错误
    if (retval) {
        fprintf(stderr,"Error setting DB num\n");
        return 1;
    }

    while(config.repeat--) {
        /* Build the command to send */
        // 创建空命令 sds
        cmd = sdsempty();
        // 包装、处理待发命令
        if (rc->flags & REDIS_CMD_MULTIBULK) {
            // MULTIBULK 批量处理
            // 包装、处理待发命令 
            // mset hello world s1 s2
            // cmd == *5 \r\n $4 \r\n mset \r\n 
            // $5 \r\n hello \r\n $5 world \r\n 
            // $2 \r\n s1 \r\n $2 \r\n s2 \r\n
            cmd = sdscatprintf(cmd,"*%d\r\n",argc);
            for (j = 0; j < argc; j++) {
                cmd = sdscatprintf(cmd,"$%lu\r\n",
                    (unsigned long)sdslen(argv[j]));
                cmd = sdscatlen(cmd,argv[j],sdslen(argv[j]));
                cmd = sdscatlen(cmd,"\r\n",2);
            }
        } else {
            for (j = 0; j < argc; j++) {
                if (j != 0) cmd = sdscat(cmd," ");
                if (j == argc-1 && rc->flags & REDIS_CMD_BULK) {
                    ownLog("%lu", strlen(argv[j])); 
                    ownLog("%s", cmd);
                    cmd = sdscatprintf(cmd,"%lu",
                        (unsigned long)sdslen(argv[j]));

                    ownLog("%s", getStrHex(cmd)); 
                } else {
                    cmd = sdscatlen(cmd,argv[j],sdslen(argv[j]));
                }
            }
            cmd = sdscat(cmd,"\r\n");
            if (rc->flags & REDIS_CMD_BULK) {
                cmd = sdscatlen(cmd,argv[argc-1],sdslen(argv[argc-1]));
                cmd = sdscatlen(cmd,"\r\n",2);
            }
        }
        anetWrite(fd,cmd,sdslen(cmd));  // 向 redis 服务端写入命令
        sdsfree(cmd);                   // 释放内存

        // 如果 monitor 执行这里
        // 监听向服务端发送的消息
        while (read_forever) {
            cliReadSingleLineReply(fd,0);
        }
        // 收取 redis 服务端的消息，返回结果
        retval = cliReadReply(fd);
        if (retval) {
            return retval;
        }
    }
    return 0;
}

static int parseOptions(int argc, char **argv) {
    int i;

    for (i = 1; i < argc; i++) {
        int lastarg = i==argc-1;

        if (!strcmp(argv[i],"-h") && !lastarg) {
            char *ip = zmalloc(32);
            // argv[i] = '-h'   argv[i+1] 就是服务地址
            // 封装的socket处理函数，把atgv[i+1]赋给ip
            if (anetResolve(NULL,argv[i+1],ip) == ANET_ERR) {
                printf("Can't resolve %s\n", argv[i]);
                exit(1);
            }
            config.hostip = ip;
            i++;
        } else if (!strcmp(argv[i],"-h") && lastarg) {
            // 如果是最后一个参数，打印帮助文档
            usage();

        } else if (!strcmp(argv[i],"-p") && !lastarg) {
            // 指定端口
            // atoi：str转int
            config.hostport = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-r") && !lastarg) {
            // -r 指定repeat重复所少次
            // strtoll 将 str转换成 long long int
            config.repeat = strtoll(argv[i+1],NULL,10);
            i++;
        } else if (!strcmp(argv[i],"-n") && !lastarg) {
            // 选择 redis 数据库
            config.dbnum = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-a") && !lastarg) {
            // 指定认证密码
            config.auth = argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-i")) {
            // 是否交互模式
            config.interactive = 1;
        } else {
            break;
        }
    }
    return i;
}

static sds readArgFromStdin(void) {
    char buf[1024];
    sds arg = sdsempty();

    while(1) {
        int nread = read(fileno(stdin),buf,1024);

        if (nread == 0) break;
        else if (nread == -1) {
            perror("Reading from standard input");
            exit(1);
        }
        arg = sdscatlen(arg,buf,nread);
    }
    return arg;
}

static void usage() {
    fprintf(stderr, "usage: redis-cli [-h host] [-p port] [-a authpw] [-r repeat_times] [-n db_num] [-i] cmd arg1 arg2 arg3 ... argN\n");
    fprintf(stderr, "usage: echo \"argN\" | redis-cli [-h host] [-a authpw] [-p port] [-r repeat_times] [-n db_num] cmd arg1 arg2 ... arg(N-1)\n");
    fprintf(stderr, "\nIf a pipe from standard input is detected this data is used as last argument.\n\n");
    fprintf(stderr, "example: cat /etc/passwd | redis-cli set my_passwd\n");
    fprintf(stderr, "example: redis-cli get my_passwd\n");
    fprintf(stderr, "example: redis-cli -r 100 lpush mylist x\n");
    fprintf(stderr, "\nRun in interactive mode: redis-cli -i or just don't pass any command\n");
    exit(1);
}

/* Turn the plain C strings into Sds strings */
static char **convertToSds(int count, char** args) {
  int j;
  char **sds = zmalloc(sizeof(char*)*count+1);

  for(j = 0; j < count; j++)
    sds[j] = sdsnew(args[j]);

  return sds;
}

static char *prompt(char *line, int size) {
    char *retval;

    do {
        printf(">> ");
        // 等待输入，把输入传入 line
        // retval 和 line 相同，都包含换行符 \n
        // fgets 无法删除输入
        retval = fgets(line, size, stdin);
    } while (retval && *line == '\n');

    // test case
    // printf ("*line=%c\n", *line);
    // printf ("retval=%s, line=%s", retval, line);

    // 把最后一个换行符\n 换成终止符\0
    line[strlen(line) - 1] = '\0';

    return retval;
}

// 交互模式
static void repl() {
    int size = 4096, max = size >> 1, argc;
    char buffer[size];
    char *line = buffer;
    char **ap, *args[max];

    if (config.auth != NULL) {
        char *authargv[2];
        // ["AUTH", "xxxx(config.auth)"]
        authargv[0] = "AUTH";
        authargv[1] = config.auth;
        // 发送命令 执行认证
        // 等于 ./redis-cli -a 命令
        cliSendCommand(2, convertToSds(2, authargv));
    }
    
    // while 循环 等待输入
    while (prompt(line, size)) {
        
        argc = 0;
        // strsep() == python split()
        for (ap = args; (*ap = strsep(&line, " \t")) != NULL;) {
            if (**ap != '\0') {
                if (argc >= max) break;
                 // strcasecmp() 忽略大小比较
                if (strcasecmp(*ap,"quit") == 0 || strcasecmp(*ap,"exit") == 0)
                    exit(0);
                ap++;
                argc++;
            }
        }

        config.repeat = 1;

        // 获取输入命令，发送到服务端
        cliSendCommand(argc, convertToSds(argc, args));
        line = buffer;
    }

    exit(0);
}

int main(int argc, char **argv) {   // 保存外部传入参数
    int firstarg;
    char **argvcopy;
    struct redisCommand *rc;

    config.hostip = "127.0.0.1";
    config.hostport = 6379;
    config.repeat = 1;
    config.dbnum = 0;               // redis 16个db 0-15
    config.interactive = 0;
    config.auth = NULL;             // 认证密码

    // 解析选项，更新config
    firstarg = parseOptions(argc,argv);
    // 正常情况 firstarg 应该等于argc
    argc -= firstarg;
    argv += firstarg;

    if (argc == 0 || config.interactive == 1) repl();

    argvcopy = convertToSds(argc, argv);

    /* Read the last argument from stdandard input if needed */
    if ((rc = lookupCommand(argv[0])) != NULL) {
      if (rc->arity > 0 && argc == rc->arity-1) {
        sds lastarg = readArgFromStdin();
        argvcopy[argc] = lastarg;
        argc++;
      }
    }

    return cliSendCommand(argc, argvcopy);
}
