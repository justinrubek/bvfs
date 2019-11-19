#ifndef FILES_H
#define FILES_H 

#include <stdbool.h>
 
typedef struct FileRecord {
    bool open;
    // Everything that follows only valid if open
    INode* node;
    bool read_only;

    int wcursor;
    int rcursor;
    
} FileRecord;

// Keep track of open files
FileRecord files[256];

void init_file_records() {
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;

        file->open = false;
        file->node = NULL;
        file->read_only = true;
    }
}

void free_file_records() {
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;

        if (file->open) {
            free(file->node);
            file->open = false;
        }
    }
}

int file_open(unsigned char inode_id, bool read_only) {
    FileRecord* file = files + inode_id;

    if (file->open == true) {
        LOG_ERROR(" is already open");
        return -1;
    }
    file->open = true;

    // Get the block the node is located at
    PtrBlock inodes = get_inodes();
    BlockID id = *(inodes + inode_id);
    LOG("inode_id %u located in block %hu\n", inode_id, id);

    // Load the block and check the filename
    Block* block = block_read(id);
    file->node = (INode*) block;
    file->read_only = read_only;

    file->wcursor = 0;
    file->rcursor = 0;
}

int file_write(unsigned char inode_id, const void* buffer, int len) {
    LOG("file_write(%u, .., %d)\n", inode_id, len);
    FileRecord* file = files + inode_id;

    if (file->open == false) {
        LOG_ERROR("File %hu not open\n", inode_id);
        return -1;
    }

    if (file->read_only == true) {
        LOG_ERROR("File %hu open in read-only mode\n", inode_id);
        return -1;
    }

    printf("inode %u has block_count %hu\n", inode_id, file->node->block_count);
    if (file->node->block_count == 0) {
        // Get an initial block for the file data
        BlockID reserved = get_free_block_id();
        printf("inode %u has no block, reserved %hu\n", inode_id, reserved);
        file->node->blocks[0] = reserved;
        file->node->block_count = 1;
    }

    int len_written = 0;

    LOG("writing for inode_id %u \n", inode_id);
    while (len != 0) {
        // Determine which block the cursor is currently on
        // TODO: Not read from the block count, but the value
        // in the block array referenced by the count
        // int block_num = file->node->block_count - 1;
        int block_index = file->node->block_count - 1;
        int block_num = file->node->blocks[block_index];
        LOG(" inode block[%d] is %d\n", block_index, block_num);
        // Read it in to memory
        Block* block = block_read(block_num);

        void* block_cursor = block + file->node->block_cursor; 
        const void* buf_cursor = buffer + len_written;
        // Determine how much space is left in the block
        int space = BLOCK_SIZE - file->node->block_cursor;
        if (space < len) {
            // This block can't fit it all, copy all that can
            memcpy(block_cursor, buf_cursor, space);
            buffer += space;
            len -= space;
            len_written += space;
            
            // Write the block data to disk
            block_write(block, block_num);
            // Fetch a new block from the superblock
            file->node->blocks[block_num+1] = get_free_block_id();
            file->node->block_count += 1;
            file->node->block_cursor = 0;
        } else {
            // This block is sufficient, copy the data over
            memcpy(block_cursor, buf_cursor, len);
            buffer += len;
            file->node->block_cursor += len;
            len_written += len;

            // Write the data to disk
            block_write(block, block_num);
            free(block);
            break;
        }

        free(block);
    }

    // Write the updated inode to disk
    // FIXME: Inode id is not the same as block id, this will overwrite
    // the superblock with an inode id of 0
    block_write(file->node, inode_id);
    return len_written;
}

/*
    TODO?
bool is_inode_open(unsigned char inode_id) {
    
}
*/

bool is_file_open(const char* file_name) {
    for (unsigned short i = 0; i < 256; ++i) {
        FileRecord* file = files + i;        

        if (file->open == false) continue;

        if (strncmp(file_name, file->node->name, MAX_FILE_NAME_LEN) == 0) {
            return true;
        }
    }

    return false;
}
 
#endif /* FILES_H */
