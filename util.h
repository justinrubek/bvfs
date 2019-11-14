#ifndef UTIL_H
#define UTIL_H 

#include <unistd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LOG(...) printf(__VA_ARGS__)

#define SUPERBLOCK_ID 0

typedef void* Block;

int file_system = -1;
void open_file_system(const char* name) {
    file_system = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
}

int block_position(int block_id) {
    return block_id * BLOCK_SIZE;
}

int fs_seek(int block_id) {
    int position = block_position(block_id);
    int res = lseek(file_system, position, SEEK_SET);

    if (res != position) {
        LOG("Failed to seek\n");
        return -1;
    } 
    return res;
}

int block_write(Block block, int block_id) {
    // Seek to the position in our fs
    int res = fs_seek(block_id);

    // Check if it succeeded
    if (res == -1) {
        return errno;
    } 

    // Write the data
    res = write(file_system, block, BLOCK_SIZE);
    if (res != BLOCK_SIZE) {
        LOG("Failed to write block %d", block_id);
        return errno;
    }

    // TODO: Clean up block memory?
    // TODO: Should we return something different?
    return block_id;
}

Block block_read(int block_id) {
    if (block_id >= BLOCK_COUNT) {
        // TODO: Make a set of return code definitions in a separate file
        return NULL;
    }
    // Allocate memory for the block 
    void* block = malloc(BLOCK_SIZE);

    // Read the data from disk
    int res = fs_seek(block_id);
    if (res == -1) {
        LOG("Failed to seek for read");
    }

    res = read(file_system, block, BLOCK_SIZE);
    if (res == -1) {
        LOG("Failed to read block %d", block_id);
    }

    // Return the buffer
    return block;
}

unsigned short get_free_block_id() {
    // Walk along the superblock and find a indirection block 
    Block superblock = block_read(SUPERBLOCK_ID);
    for (unsigned short i = 0; i < 256; ++i) {
        if (superblock[i] != 0) {
            // We found a valid indirection block
            // Navigate through indirection to see if there's an address to use

        }
    }

    // Cleanup memory (superblock)
    free(superblock);
}

void filesystem_create(const char* name, int size) {
    open_file_system(name);

    // Seek to end and place byte
    char block[BLOCK_SIZE];
    int pos = 0;
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        block[pos++] = 0;
    }
    fs_seek(BLOCK_COUNT-1);
    block_write(&block, SUPERBLOCK_ID);

    // TODO: Prepare superblock
    unsigned short superblock[BLOCK_SIZE];
    // Keep building blocks until we've encountered all addresses

    unsigned short currentBlock[256]; // The block we're filling with addresses
    int currentBlockId = 2;
    int currPos = 0; // position in currentBlock
    int superPos = 0;

    LOG("Preparing superblock\n");
    for (unsigned short i = 3; i < BLOCK_COUNT; ++i) {
        if (currPos == 256) {
            LOG("Block prepared, adding to superblock\n");
            // Write current block and get a new one
            block_write(currentBlock, currentBlockId);
            superblock[superPos++] = currentBlockId;

            currentBlockId = i++; 
            currPos = 0;
            // Check if this goes past end
            if (currentBlockId == BLOCK_COUNT) {
                break;
            }
        }
        LOG("Block %hu referencing %hu\n", currentBlockId, i);
        currentBlock[currPos++] = i;
    }

    // fs_seek(SUPERBLOCK_ID);
    block_write(&superblock, SUPERBLOCK_ID);
    LOG("Superblock written\n");

    close(file_system);
}

#endif /* UTIL_H */
