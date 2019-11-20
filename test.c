#include <stdio.h>

#include "bvfs.h"

#define DEBUG

int main() {
    LOG("Logging enabled\n");
    const char* name = "partition.bvfs";
    bv_init(name);

    bv_ls();

//    block_write_offset("pizza is the best", 17, 1, 5);

    /*
    int fd0 = bv_open("pizza.txt", BV_WCONCAT);
    printf("Received fd: %d\n", fd0);
    int len = bv_write(fd0, "PPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingPizza is amazingizza is amazing",
        510);
    bv_write(fd0, "Second part", 11);
    */

    int fd = bv_open("pizza.txt", BV_RDONLY);
    char buf[550];
    bv_read(fd, buf, 520);
    printf("Data: [%.520s]\n", buf);

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

