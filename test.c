#include <stdio.h>

#include "bvfs.h"

int main() {
    printf("%d\n", PARTITION_SIZE);
    bv_init("partition.bvfs");

  return 0;
}

