#include <stdio.h>

int main() {
    unsigned char a = 0x80;
    unsigned char *p = a;
    unsigned int len = *p;
    
    printf("%hhn\n", p);
    printf("%d\n", len);
}

