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

#ifndef DEBUG
    #define LOG(...) do { } while(0)
#else
    #define LOG(...) printf(__VA_ARGS__)
#endif // DEBUG

#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)

#define SUPERBLOCK_ID 0
#define INODE_LIST_ID 1

typedef unsigned short* PtrBlock;
typedef unsigned short BlockID;


// TODO: Add timestamp and adjust size of padding accordingly
typedef struct INode {
    char name[MAX_FILE_NAME_LEN];
    unsigned short block_count;
    unsigned short block_cursor; // Cursor of the farthest block
    unsigned short blocks[FILE_BLOCK_COUNT];
    char padding[512 - (MAX_FILE_NAME_LEN + 4 + (FILE_BLOCK_COUNT/2))];
} INode;

// Prepare a portion of memory to be a fresh inode
void create_inode(void* buf, const char* name) {
    INode* inode = (INode*)buf; 
    strncpy(inode->name, name, MAX_FILE_NAME_LEN);

    inode->block_count = 0;
    inode->block_cursor = 0;
    for (int i = 0; i < FILE_BLOCK_COUNT; ++i) {
        inode->blocks[i] = 0;
    }
    char* padding = inode->padding;
    for (int i = 0; i < sizeof(inode->padding); ++i) {
        *(padding+1) = 0;
    }
    
}

// Global file descriptor of our partition
int file_system = -1;

typedef struct Block {
    char bytes[BLOCK_SIZE];
} Block;

// Set all values in a block-sized piece of memory to 0
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

// Calculate where in the partition the block exists
int block_position(int block_id) {
    return block_id * BLOCK_SIZE;
}

// Seek to the start of a block in our partition
int fs_seek(int block_id) {
    int position = block_position(block_id);
    LOG("Seeking to %d\n", position);
    int res = lseek(file_system, position, SEEK_SET);

    if (res != position) {
        LOG_ERROR("Failed to seek for block %d pos %d\n", block_id, position);
        return -1;
    } 
    return res;
}

// Given 512 bytes of data and a block number, seek through the partition and write the block
int block_write(void* block, int block_id) {
    LOG("Writing block %d\n", block_id);
    // Seek to the position of the block in our fs
    int res = fs_seek(block_id);

    // Check if it succeeded
    if (res == -1) {
        return errno;
    } 

    // Write the data
    res = write(file_system, block, BLOCK_SIZE);
    if (res != BLOCK_SIZE) {
        LOG_ERROR("Failed to write block %d", block_id);
        return errno;
    }

    // TODO: Should we return something different?
    return block_id;
}

// Retrieve a heap allocated buffer to the data contained by the block
Block* block_read(int block_id) {
    if (block_id >= BLOCK_COUNT) {
        LOG_ERROR("Tried to read invalid block %hu", block_id);
        return NULL;
    }
    // Allocate memory for the block 
    void* block = malloc(BLOCK_SIZE);
    zero_block(block);

    // Read the data from disk
    int res = fs_seek(block_id);
    if (res == -1) {
        // LOG_ERROR("Failed to seek for read");
        return NULL;
    }

    res = read(file_system, block, BLOCK_SIZE);
    if (res == -1) {
        LOG_ERROR("Failed to read block %d", block_id);
        return NULL;
    }

    // Return the buffer
    return (Block*)block;
}

// Load the block into memory and write the data at a given offset to the block
// Use this to avoid having the manually load in the block before copying data
int block_write_offset(const char* data, int len, int block_id, int offset) {
    LOG("block_write_offset(.., %d, %d, %d)\n", len, block_id, offset);
    if (offset + len > BLOCK_SIZE) {
        LOG_ERROR("Attempted to write past end of block\n");
        return -1;
    }

    // Load the block into memory
    Block* block = block_read(block_id);

    // Advance our reference to the block so we write to the proper spot
    void* cursor = block + offset;

    // Perform copy of data
    for (int i = 0; i < len; ++i) {
        block->bytes[offset + i] = data[i];
    }

    // Write the block to disk
    block_write(block, block_id);
    free(block);

    return len;
}


Block* superblock_global = NULL;
// Retrieve the superblock. 
// Allows us to share the block without worrying who needs to free memory
Block* get_superblock() {
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

// Walk through the superblock structure and find a free block to use, removing it from the structure
unsigned short get_free_block_id() {
    // Walk along the superblock and find a indirection block 
    PtrBlock superblock = (PtrBlock) get_superblock();
    for (unsigned short i = 0; i < 256; ++i) {
        if (superblock[i] != 0) {
            // We found a valid indirection block
            // Navigate through indirection to see if there's an address to use
            unsigned short block_id = superblock[i];
            LOG("Looking at indirection block %hu at block %hu\n", i, block_id);
            PtrBlock block = (PtrBlock) block_read(block_id);
            for (unsigned short j = 0; j < 256; ++j) {
                if (block[j] != 0) {
                    LOG("   Found free block: indirection %hu[%hu] = %hu\n", block_id, j, block[j]);
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

// Create the partition and add initial metadata
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
    int currentBlockId = 257;
    int currPos = 0; // position in currentBlock
    int superPos = 0;

    for (unsigned short i = 258; i < BLOCK_COUNT; ++i) {
        if (currPos == 256) {
            // LOG("Block prepared, adding to superblock\n");
            // Write current block and get a new one
            block_write(currentBlock, currentBlockId);
            superblock[superPos++] = currentBlockId;

            currentBlockId = i++; 
            currPos = 0;
            zero_block(currentBlock);
            // Check if this goes past end
            if (currentBlockId == BLOCK_COUNT) {
                break;
            }
        }

        // LOG("Block %hu referencing %hu\n", currentBlockId, i);
        currentBlock[currPos++] = i;
    }

    block_write(superblock, SUPERBLOCK_ID);
    // LOG("Superblock written\n");
    

    // close(file_system);
}

#endif /* UTIL_H */
