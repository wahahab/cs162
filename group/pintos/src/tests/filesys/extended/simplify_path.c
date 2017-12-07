#include "tests/main.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"

void test_path(char *path_, int success)
{
    char path[1024];
    int i;
    
    for (i = 0 ; *(path_ + i) != '\0' ; i++) {
        path[i] = *(path_ + i);
    }
    path[i] = '\0';
    CHECK(simplify_path(path) == success,
            "simplifying path '%s'", path);
    msg("simplified path: '%s'", path);
}

void
test_main(void)
{
    test_path("/foo/bar///../../././path/foo/.././", 1);
    test_path("ath/foo/.././", 0);
    test_path("/foo/.././../../", 0);
    test_path("/", 1);
    test_path("", 0);
}
