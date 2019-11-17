#include <stdio.h>

#include "bvfs.h"

int main() {
    printf("%d\n", PARTITION_SIZE);
    const char* name = "partition.bvfs";
    bv_init(name);

    int fd0 = bv_open("pizza.txt", BV_WCONCAT);
    printf("Received fd: %d\n", fd0);

    int fd1 = bv_open("pizza2.txt", BV_WCONCAT);
    printf("Received fd: %d\n", fd1);

    int fd2 = bv_open("pizza3.txt", BV_WCONCAT);
    printf("Received fd: %d\n", fd2);
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

