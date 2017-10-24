#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


int main() {
    
    int *stuff = malloc(sizeof(int) * 1);
    
    *stuff = 7;
    pid_t pid = fork();
    printf("Stuff is %d\n", *stuff);
    if(pid == 0)
        *stuff = 6;

    return 0;
}
