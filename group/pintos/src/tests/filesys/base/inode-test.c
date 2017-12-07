#include "tests/lib.h"
#include "tests/main.h"
#include "filesys/inode.c"

void test_main(void)
{
    block_sector_t sector = 10;
    struct inode *inode;

    // Test inode create
    inode_create(sector, 3356);
    inode = inode_open(sector);
    CHECK(inode->data.length == 3356,
            "inode's length should equal to initial size");
}
