#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static char *prompt(char *line, int size) {
    char *retval;

    do {
        printf(">> ");
        // 等待输入，把输入传入 line
        retval = fgets(line, size, stdin);
    } while (retval && *line == '\n');

    line[strlen(line) - 1] = '\0';
    
    return retval;
}

static void repl(){
    int size = 4096, max = size >> 1, argc;
    char buffer[size];
    char *line = buffer;
    char **ap, *args[max];

    // while 循环 等待输入
    while (prompt(line, size)) {
        
        argc = 0;

        for (ap = args; (*ap = strsep(&line, " \t")) != NULL;) {
            if (**ap != '\0') {
                if (argc >= max) break;
                // strcasecmp 忽略大小比较
                if (strcasecmp(*ap,"quit") == 0 || strcasecmp(*ap,"exit") == 0)
                    exit(0);
                ap++;
                argc++;
            }
        }

        // config.repeat = 1;

        // 获取输入命令，发送到服务端
        printf("参数个数：%d\n", argc);
        for(int i = 0; i < argc; i++){
            printf("参数：%s\n", args[i]);
        }


        line = buffer;
    }

    exit(0);
}

int main(int argc, char** argv){
    repl(); // 进入循环读取命令
    return 0;
}