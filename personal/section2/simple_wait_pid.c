#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


int main() {

    int exit;
    pid_t pid = fork();

    if (pid != 0) {
        waitpid(pid, &exit, 0);
    }
    printf("Hello world %d\n", pid);

    return 0;

}
