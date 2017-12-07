#include "filesys/path.h"

void path_basename(const char *path, char* fname)
{
    int i;
    int j;
    
    for (i = 0, j = 0 ; *(path + i) != '\0' ; i ++) {
        if (*(path + i) != '/') {
            *(fname + j) = *(path + i);
            j++;
        }
        else {
            *(fname + j) = '\0';
            j = 0;
        }
    }
    if (j != 0)
        *(fname + j) = '\0';
}
