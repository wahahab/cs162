#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    struct rlimit nlim;

    nlim.rlim_cur = 2782;

    getrlimit(RLIMIT_STACK, &lim);
    printf("stack size: %lld\n", (long long) lim.rlim_cur);
    // setrlimit(RLIMIT_NPROC, &lim);
    getrlimit(RLIMIT_NPROC, &lim);
    prlimit(getpid(), RLIMIT_NPROC, nlim, lim);
    getrlimit(RLIMIT_NPROC, &lim);
    printf("process limit: %lld\n", (long long) lim.rlim_cur);
    getrlimit(RLIMIT_NOFILE, &lim);
    printf("max file descriptors: %lld\n", (long long) lim.rlim_cur);
    return 0;
}
