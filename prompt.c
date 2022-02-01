#include <stdio.h>
#include <string.h>

static char *prompt(char *line, int size) {
    char *retval;

    do {
        printf(">> ");
        // 等待输入，把输入传入 line
        retval = fgets(line, size, stdin);
    } while (retval && *line == '\n');
    // printf ("*line=%c\n", *line);
    // printf ("retval=%s, line=%s", retval, line);

    line[strlen(line) - 1] = '\0';
    // printf ("retval=%s, line=%s", retval, line);
    return retval;
}

// void print(char *str){
//     while (str != '\0')
// }

int main(int argc, char **argv){
    int size = 4096, max = size >> 1;
    char buffer[size];
    char *line = buffer;
    char *res = prompt(line, size);

    return 0;
}