#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main() {

    int i;
    char** argv = (char **) malloc(sizeof(char*) * 3);

    argv[0] = "/bin/ls";
    argv[1] = ".";
    argv[2] = NULL;
    pid_t pid;

    for (i = 0 ; i < 10 ; i++) {
        printf("Number: %d\n", i);

        if (i == 3) {
    
            pid = fork();
            if (pid == 0) {
                
                execv("/bin/ls", argv);
                
            }


        }

    }

    return 0;
}
