#include <stdio.h>

#include "bvfs.h"

#define DEBUG

int main() {
    const char* name = "partition.bvfs";
    bv_init(name);

    bv_ls();

//    block_write_offset("pizza is the best", 17, 1, 5);

    int fd0 = bv_open("pizza.txt", BV_WCONCAT);
    printf("Received fd: %d\n", fd0);
    int len = bv_write(fd0, "Pizza is amazing ", 17);
    printf("Written %d bytes\n", len);
    len = bv_write(fd0, "and so is pizza", 15);
    printf("Written %d bytes\n", len);


    /*

    char nums[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        nums[i] = i;
    }

    // Test reading and writing of individual blocks
    block_write(&nums, 5);

    Block b = block_read(0);
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        if (nums[i] != i) {
            printf("not equal: %d = %d\n", i, nums[i]);
        } else {
            printf("equal: %d = %d\n", i, nums[i]);
        }
    }

*/
    bv_destroy();
    close(file_system);

  return 0;
}

