#include "tests/main.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/lib.h"

void
test_main(void)
{
    CHECK(mkdir("/foo"), "create folder 'foo'");
    CHECK(mkdir("/foo/bar"), "create folder 'bar'");
    CHECK(chdir("/foo/bar"), "chdir: /foo/bar");
    CHECK(!mkdir("/foobar/foo"), "mkdir: /foobar/foo");
    CHECK(!chdir("/foobar"), "chdir: /foobar");
    CHECK(chdir(".."), "chdir: ..");
    CHECK(mkdir("./foobar"), "mkdir: ./foobar");
    CHECK(chdir("/foo/foobar"), "chdir: /foo/foobar");
}
