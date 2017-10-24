#include <stdio.h>
#include <unistd.h>


int main() {

    pid_t pid = 1;
    pid = fork();

    printf("Child process PID is %d\n", pid);

    return 0;
}
