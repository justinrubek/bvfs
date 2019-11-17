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
#define INODE_LIST_ID 1

typedef struct INode {
    char name[MAX_FILE_NAME_LEN];
    unsigned short file_size;
    unsigned short blocks[FILE_BLOCK_COUNT];
} INode;

void create_inode(void* buf, const char* name) {
    INode* inode = (INode*)buf; 
    strncpy(inode->name, name, MAX_FILE_NAME_LEN);

    inode->file_size = 0;
    for (int i = 0; i < FILE_BLOCK_COUNT; ++i) {
        inode->blocks[i] = 0;
    }
}

typedef char* Block;

int file_system = -1;

void zero_block(void* block) {
    char* bytes = (char*) block;
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        bytes[i] = 0;
    }
}

// Create the partition when it doesn't exist
void init_file_system(const char* name) {
    file_system = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    // TODO: Report error
}

// Open an existing partition
void open_file_system(const char* name) {
    file_system = open(name, O_RDWR);
    // TODO: Report error
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

int block_write(void* block, int block_id) {
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
    zero_block(block);

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

Block superblock_global = NULL;
// Retrieve the superblock. 
// Allows us to share the block without worrying who needs to free memory
Block get_superblock() {
    // Check if we have the superblock currently loaded
    if (superblock_global == NULL) {
        // If not, load it into memory
        superblock_global = block_read(SUPERBLOCK_ID);
    }

    // Finally, return it
    return superblock_global;
}

void free_superblock() {
    if (superblock_global == NULL) return;

    free(superblock_global);
    superblock_global = NULL;
}

Block inode_global = NULL;
unsigned short* get_inodes() {
    // Check if we have the inodes currently loaded
    if (inode_global == NULL) {
        // If not, load it into memory
        inode_global = block_read(INODE_LIST_ID);
    }

    // Finally, return it
    return inode_global;
}

void inodes_write() {
    block_write(inode_global, INODE_LIST_ID);
}

void free_inodes() {
    if (inode_global == NULL) return;

    free(inode_global);
    inode_global = NULL;
}

unsigned short get_free_block_id() {
    // Walk along the superblock and find a indirection block 
    Block superblock = get_superblock();
    for (unsigned short i = 0; i < 256; ++i) {
        if (superblock[i] != 0) {
            // We found a valid indirection block
            // Navigate through indirection to see if there's an address to use
            unsigned short block_id = superblock[i];
            Block block = block_read(block_id);
            for (unsigned short j = 0; j < 256; ++j) {
                if (block[j] != 0) {
                    unsigned short id = block[j];
                    block[j] = 0; // Ensure this block is not marked as free
                    block_write(block, block_id);
                    free(block);
                    return id;
                }
            }
            free(block);
        }
    }

    return -1;
}

bool file_exists(const char* name) {
    // Walk through the inode list and see if there is a file matching
    unsigned short* list = (unsigned short*) get_inodes();
    for (int i = 0; i < 256; ++i) {
        // Check if the inode is null
        unsigned short block_id = list[i];
        if (block_id == 0) {
            continue;
        }

        // Load the block and check the filename
        Block block = block_read(block_id);
        INode* inode = (INode*) block;
        if (strncmp(inode->name, name, MAX_FILE_NAME_LEN) == 0) {
            free(block);
            return true;
        }

        free(block);
    }

    return false;
}

void filesystem_create(const char* name, int size) {
    init_file_system(name);

    // Seek to end and place byte
    char block[BLOCK_SIZE];
    zero_block(block);
    fs_seek(BLOCK_COUNT-1);
    block_write(block, SUPERBLOCK_ID);

    // Prepare superblock
    unsigned short superblock[BLOCK_SIZE];
    zero_block((char*)superblock);
    // Keep building blocks until we've encountered all addresses

    unsigned short currentBlock[256]; // The block we're filling with addresses
    zero_block((char*)currentBlock);
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
            zero_block((char*)currentBlock);
            // Check if this goes past end
            if (currentBlockId == BLOCK_COUNT) {
                break;
            }
        }

        LOG("Block %hu referencing %hu\n", currentBlockId, i);
        currentBlock[currPos++] = i;
    }

    block_write(superblock, SUPERBLOCK_ID);
    LOG("Superblock written\n");

    // close(file_system);
}

#endif /* UTIL_H */
