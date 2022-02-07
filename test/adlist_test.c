#include <stdio.h>
#include "../redis-1.3.6-src/adlist.c"
#include "../redis-1.3.6-src/zmalloc.c"

void print_list(list *list) {
    listIter *iter;
    listNode *node;

    
    iter = listGetIterator(list, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL){
        printf("%d ", *((int *)node->value));
    }
    printf("\n");
    listReleaseIterator(iter);
}


int main() {
    list *list;
    int l = 1;
    list = listCreate();
    list = listAddNodeHead(list, &l);
    l = 2;
    list = listAddNodeHead(list, &l);
    l = 4;
    list = listAddNodeHead(list, &l);
    print_list(list);
    return 0;
}