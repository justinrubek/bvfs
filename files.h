#ifndef FILES_H
#define FILES_H 

#include <stdbool.h>
 
typedef struct FileRecord {
    bool open;
    // Everything that follows only valid if open
    INode* node;
    bool read_only;

    int cursor; // Cursor for reading
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
    file->cursor = 0;
}

void print_inode(INode* node) {
    printf("INode{\n");
    printf("    name %s\n", node->name);
    printf("    block_count %hu\n", node->block_count);
    printf("    block_cursor %hu\n", node->block_cursor);
    printf("}\n");
}

int file_read(unsigned char inode_id, void* buffer, int len) {
    LOG("file_read(%u, .., %d)\n", inode_id, len);
    FileRecord* file = files + inode_id;

    if (file->open == false) {
        LOG_ERROR("File %hu not open\n", inode_id);
        return -1;
    }

    if (file->node->block_count == 0
        || (file->node->block_count == 1 && file->node->block_cursor == 0) ) {
        // No data to be had
        return 0;
    }

    int len_read = 0;

    while (len_read != len) {
        // Determine which block the cursor is currently on
        // TODO: Not read from the block count, but the value
        // in the block array referenced by the count
        // int block_num = file->node->block_count - 1;
        int block_index = file->cursor / BLOCK_SIZE;
        int block_num = file->node->blocks[block_index];

        const void* buf_cursor = buffer + len_read;
        // Determine how much space is left in the block
        int block_cursor = file->cursor % BLOCK_SIZE;
        int space = BLOCK_SIZE - block_cursor;

        Block* block = block_read(block_num);

        if (space < len - len_read) {
            LOG("Not enough data in this block %d leaves only %d\n", file->cursor, space);
            memcpy(buf_cursor, block->bytes + block_cursor, space);
            len_read += space;
            file->cursor += space;
            
        } else {
            LOG("Copying all data over into block\n");
            memcpy(buf_cursor, block->bytes + block_cursor, len - len_read);
            // Keep track of how much we've read
            len_read = len;
            file->cursor += len - len_read;
        }

        free(block);
    }

    return len_read;

}

void inode_write(unsigned char inode_id) {
    INode* node = (files + inode_id)->node;

    // TODO: Update the timestamp

    // Write to disk
    block_write(node, inode_id + 1);
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
        print_inode(file->node);
        // Determine which block the cursor is currently on
        // TODO: Not read from the block count, but the value
        // in the block array referenced by the count
        // int block_num = file->node->block_count - 1;
        int block_index = file->node->block_count - 1;
        int block_num = file->node->blocks[block_index];
        LOG(" inode block[%d] is %d\n", block_index, block_num);

        const void* buf_cursor = buffer + len_written;
        // Determine how much space is left in the block
        int space = BLOCK_SIZE - file->node->block_cursor;
        if (space < len) {
            LOG("Not enough space in this block, must expand\n");
            // This block can't fit it all, copy all that can
            block_write_offset(buf_cursor, space, block_num, file->node->block_cursor);
            len -= space;
            len_written += space;
            
            // Fetch a new block from the superblock
            file->node->block_count += 1;
            file->node->blocks[block_index + 1] = get_free_block_id();
            file->node->block_cursor = 0;
        } else {
            LOG("Copying all data over into block\n");
            // This block is sufficient, copy the data over
            block_write_offset(buf_cursor, len, block_num, file->node->block_cursor);
            // Keep track of how much we've written
            file->node->block_cursor += len;
            len_written += len;
            len = 0;
        }
    }

    // Write the updated inode to disk
    inode_write(inode_id + 1);
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
