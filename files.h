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

    // Get the block the node is located at
    PtrBlock inodes = get_inodes();
    BlockID id = *(inodes + inode_id);

    // Load the block and check the filename
    Block block = block_read(id);
    file->node = (INode*) block;
    file->read_only = read_only;

    file->wcursor = 0;
    file->rcursor = 0;
}

int file_write(unsigned char inode_id, const void* buffer, int len) {
    FileRecord* file = files + inode_id;

    if (file->open == false) {
        LOG_ERROR("File %hu not open\n", inode_id);
        return -1;
    }

    if (file->read_only == true) {
        LOG_ERROR("File %hu open in read-only mode\n", inode_id);
        return -1;
    }

    if (file->node->block_count == 0) {
        // Get an initial block for the file data
        file->node->blocks[0] = get_free_block_id();
    }

    int blocks_to_write = len;

    // Determine which block the cursor is currently on
    while (len != 0) {
        int block_num = file->node->block_count - 1;
        Block block = block_read(block_num);

        // Determine how much space is left in the block
        int space = BLOCK_SIZE - file->node->block_cursor;
        if (space < len) {
            // This block can't fit it all
            memcpy(block + file->node->block_cursor, buffer, space);
            buffer += space;
            len -= space;
            
            // Write the data
            block_write(block, block_num);
            // Fetch a new block from the superblock
            file->node->blocks[block_num+1] = get_free_block_id();
            file->node->block_count += 1;
            file->node->block_cursor = 0;
        } else {
            // This block is sufficient
            memcpy(block + file->node->block_cursor, buffer, len);
            buffer += len;
            file->node->block_cursor += len;

            // Write the data
            block_write(block, block_num);
            free(block);
            break;
        }

        free(block);
    }

    block_write(file->node, inode_id);
    return blocks_to_write - len;
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
