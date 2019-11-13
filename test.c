#include <stdio.h>

#include "bvfs.h"

int main() {
    printf("%d\n", PARTITION_SIZE);
    const char* name = "partition.bvfs";
    bv_init(name);
    
    open_file_system(name);

    char nums[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        nums[i] = i;
    }

    // Test reading and writing of individual blocks
    block_write(&nums, 5);

    Block b = block_read(0);
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        /*
        if (nums[i] != i) {
            printf("not equal: %d = %d\n", i, nums[i]);
        } else {
            printf("equal: %d = %d\n", i, nums[i]);
        }
        */
    }

    close(file_system);

  return 0;
}

