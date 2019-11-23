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
    #define LOG_FUNC(...) do { } while(0)
#else
    #define LOG(...) printf(__VA_ARGS__)
    #define LOG_FUNC(func_call) func_call
#endif // DEBUG

#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)

#define SUPERBLOCK_ID 0

typedef unsigned short* PtrBlock; // A block containing 'pointers' to other blocks as a array of 256 ints
typedef unsigned short BlockID;

typedef struct Block {
    char bytes[BLOCK_SIZE];
} Block;

typedef struct INode {
    char name[MAX_FILE_NAME_LEN];
    time_t timestamp;
    unsigned short block_count;
    unsigned short block_cursor; // Cursor of the farthest block
    unsigned short blocks[FILE_BLOCK_COUNT];
    char padding[512 - (MAX_FILE_NAME_LEN + sizeof(time_t) + 4 + (FILE_BLOCK_COUNT/2))];
} INode;

// Prepare a given portion of memory to be a fresh inode
void create_inode(void* buf, const char* name) {
    INode* inode = (INode*) buf; 
    strncpy(inode->name, name, MAX_FILE_NAME_LEN);

    inode->timestamp = time(NULL);

    inode->block_count = 0;
    inode->block_cursor = 0;
    for (int i = 0; i < FILE_BLOCK_COUNT; ++i) {
        inode->blocks[i] = 0;
    }
}

// Set all values in a block-sized piece of memory to 0
void zero_block(void* block) {
    char* bytes = (char*) block;
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        bytes[i] = 0;
    }
}

// Global file descriptor of our partition
int file_system = -1;

// Create the partition when it doesn't exist
void init_file_system(const char* name) {
    file_system = open(name, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (file_system == -1) {
        LOG_ERROR("Failed to create partition\n");
    }
}

// Open an existing partition
void open_file_system(const char* name) {
    file_system = open(name, O_RDWR);
    if (file_system == -1) {
        LOG_ERROR("Failed to open partition\n");
    }
}

// Calculate where in the partition the block exists
int block_position(int block_id) {
    return block_id * BLOCK_SIZE;
}

// Seek to the start of a block in our partition
int fs_seek(int block_id) {
    int position = block_position(block_id);
    // LOG("Seeking to %d\n", position);
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

// Retrieve block data into a given buffer
int block_read_buf(void* buf, int block_id) {
    // Read the data from disk
    int res = fs_seek(block_id);
    if (res == -1) {
        // LOG_ERROR("Failed to seek for read");
        return -1;
    }

    res = read(file_system, buf, BLOCK_SIZE);
    if (res != BLOCK_SIZE) {
        LOG_ERROR("Failed to read block %d\n", block_id);
        return -1;
    }

    return 0;
}

// Retrieve a heap allocated buffer to the data contained by the block
Block* block_read(int block_id) {
    if (block_id >= BLOCK_COUNT) {
        LOG_ERROR("Tried to read invalid block %hu\n", block_id);
        return NULL;
    }
    // Allocate memory for the block 
    void* block = malloc(BLOCK_SIZE);
    
    int res = block_read_buf(block, block_id);
    if (res != 0) {
        free(block);
        return NULL;
    }

    // Return the buffer
    return (Block*) block;
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

void write_superblock() {
    Block* superblock = get_superblock();

    block_write(superblock, SUPERBLOCK_ID);
}

void free_superblock() {
    if (superblock_global == NULL) return;

    free(superblock_global);
    superblock_global = NULL;
}

// Walk through a superblock indirection block to find a free space
BlockID get_free_block_id_progress(int index) {
    LOG("Looking at indirection block %d\n", index);
    PtrBlock block = (PtrBlock) block_read(index);
    for (int i = 0; i < 256; ++i) {
        BlockID id = block[i];

        if (id != 0) {
            // LOG("   Found free block: indirection %hu[%d] = %hu\n", index, i, id);
            block[i] = 0; // Ensure this block is not marked as free
            block_write(block, index);
            free(block);
            return id;
        }
    }
    free(block);

    LOG("Failed to find a free block in block %hu\n", index);
    return -1;
}

// Walk through the superblock structure and find a free block to use, removing it from the structure
BlockID get_free_block_id() {
    LOG("get_free_block_id()\n");
    // Walk along the superblock and find a indirection block 
    PtrBlock superblock = (PtrBlock) get_superblock();
    for (int i = 0; i < 256; ++i) {
        BlockID sid = superblock[i];
        // LOG(" sid = %hu\n", sid);

        // We found a valid indirection block
        if (sid != 0) {
            BlockID res = get_free_block_id_progress(sid);
            if (res != -1) {
                return res;
            }
        }
    }

    LOG_ERROR("Failed to find a free block id\n");
    return -1;
}




/*
 * bool free_disk_block_progress(BlockID id, int superblock_index);
 *
 * Frees a block by looking for a place to reference it in a given PtrBlock
 * The given block id must contain a block with 2 byte numbers referencing
 * locations on disk. Free spaces will be denoted with the value of 0
 * 
 * Input Parameters
 *   id: The block index to be placed as a reference
 *   index: The block index of the PtrBlock to be referenced
 *
 * Return Value
 *   int:  0 if the initialization succeeded.
 *        -1 if the initialization failed (eg. file not found, access denied,
 *           etc.). Also, print a meaningful error to stderr prior to returning.
 */
bool free_disk_block_progress(BlockID id, int index) {
    PtrBlock block = (PtrBlock) block_read(index);

    // Find a free space in the block and place id in it
    for (int i = 0; i < 256; ++i) {
        BlockID bid = block[i]; 

        // Free space found
        if (bid == 0) {
            // Free space located 
            block[i] = id;
            block_write(block, index);
            free(block);
            return true;
        }
    }

    free(block);
    return false;
}

// Attempt to add a given block into the superblock pool
bool free_disk_block(BlockID id) {
    PtrBlock superblock = (PtrBlock) get_superblock();

    for (int i = 0; i < 256; ++i) {
        BlockID sid = superblock[i];

        // If this is an empty reference, make it a reference to this block
        if (sid == 0) {
            superblock[i] = id;
            write_superblock();
            return true;
        }

        // Attempt to place the block inside the referenced block
        bool res = free_disk_block_progress(id, sid);
        if (res) {
            return true;
        }
    }

    // Failed to locate a place for the block to live
    LOG_ERROR("Potential disk leak error\n");
    return false;
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
    superblock_global = (Block*) malloc(BLOCK_SIZE);
    PtrBlock superblock = (PtrBlock) superblock_global;
    zero_block((char*)superblock);

    // Keep building blocks until we've encountered all addresses
    unsigned short currentBlock[256]; // The block we're filling with addresses
    zero_block((char*)currentBlock);
    unsigned short currentBlockId = 257;
    int currPos = 0; // position in currentBlock
    int superPos = 0;

    // Loop through all open blocks and add to the superblock
    // The superblock will contain references to blocks that
    // themselves hold references to free data blocks.
    for (int i = 258; i < BLOCK_COUNT; ++i) {
        if (currPos == 256) {
            // Write current block and get a new one
            // LOG("superblock[%d] = %hu\n", superPos, currentBlockId);
            block_write(currentBlock, currentBlockId);
            superblock[superPos++] = currentBlockId;

            currentBlockId = i++; 
            currPos = 0;
            // Check if this goes past end
            if (currentBlockId == BLOCK_COUNT) {
                break;
            }
        }

        // LOG("   >ref%d[%d] <= %d\n", currentBlockId, currPos, i);
        currentBlock[currPos++] = i;
    }

    write_superblock();
}

#endif /* UTIL_H */
