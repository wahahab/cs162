/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    int *data;
    int i;

    for (i = 1; i < 1024; i++) {
        data = (int*) mm_malloc(i);
        data[0] = 1;
    }
    printf("malloc sanity test successful!\n");
    return 0;
}
