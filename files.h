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
        file->node = block_read(i + 1); // Read inode from disk
        file->read_only = true;
    }
}

void free_file_records() {
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;

        if (file->open) {
            block_write(file->node, i + 1); // Write inode to disk
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
            LOG("Not enough space in this block, must expand\n");
            // This block can't fit it all, copy all that can
            memcpy(block_cursor, buf_cursor, space);
            buffer += space;
            len -= space;
            len_written += space;
            file->wcursor += space;
            
            // Write the block data to disk
            block_write(block, block_num);
            // Fetch a new block from the superblock
            file->node->blocks[block_num+1] = get_free_block_id();
            file->node->block_count += 1;
            file->node->block_cursor = 0;
        } else {
            LOG("Copying all data over into block\n");
            // This block is sufficient, copy the data over
            memcpy(block_cursor, buf_cursor, len);
            file->wcursor += space;
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
    // block_write(file->node, inode_id+1);
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

int file_inode_id(const char* name) {
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;        

        if (strncmp(name, file->node->name, MAX_FILE_NAME_LEN) == 0) {
            return 0;
        }
    }

    return -1;
}
 
#endif /* FILES_H */
