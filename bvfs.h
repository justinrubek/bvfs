/* CMSC 432 - Homework 7
 * Assignment Name: bvfs - the BV File System
 * Due: Thursday, November 21st @ 11:59 p.m.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "bvfs_constants.h"
#include "util.h"
#include "files.h"


/*
 * [Requirements / Limitations]
 *   Partition/Block info
 *     - Block Size: 512 bytes
 *     - Partition Size: 8,388,608 bytes (16,384 blocks)
 *
 *   Directory Structure:
 *     - All files exist in a single root directory
 *     - No subdirectories -- just names files
 *
 *   File Limitations
 *     - File Size: Maximum of 65,536 bytes (128 blocks)
 *     - File Names: Maximum of 32 characters including the null-byte
 *     - 256 file maximum -- Do not support more
 *
 *   Additional Notes
 *     - Create the partition file (on disk) when bv_init is called if the file
 *       doesn't already exist.
 */



// Prototypes
int bv_init(const char *fs_fileName);
int bv_destroy();
int bv_open(const char *fileName, int mode);
int bv_close(int bvfs_FD);
int bv_write(int bvfs_FD, const void *buf, size_t count);
int bv_read(int bvfs_FD, void *buf, size_t count);
int bv_unlink(const char* fileName);
void bv_ls();


/*
 * int bv_init(const char *fs_fileName);
 *
 * Initializes the bvfs file system based on the provided file. This file will
 * contain the entire stored file system. Invocation of this function will do
 * one of two things:
 *
 *   1) If the file (fs_fileName) exists, the function will initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 *   2) If the file (fs_fileName) does not exist, the function will create that
 *   file as the representation of a new file system and initialize in-memory
 *   data structures to help manage the file system methods that may be invoked.
 *
 * Input Parameters
 *   fs_fileName: A c-string representing the file on disk that stores the bvfs
 *   file system data.
 *
 * Return Value
 *   int:  0 if the initialization succeeded.
 *        -1 if the initialization failed (eg. file not found, access denied,
 *           etc.). Also, print a meaningful error to stderr prior to returning.
 */
int bv_init(const char* partitionName) {
    if (access(partitionName, F_OK) != -1) {
        // Exists
        LOG("Partition file exists\n");
        open_file_system(partitionName);
    } else {
        LOG("Creating partition file\n");
        // Needs to be created
        filesystem_create(partitionName, PARTITION_SIZE);
    }

    init_file_records();

    return 0;
}

/*
 * int bv_destroy();
 *
 * This is your opportunity to free any dynamically allocated resources and
 * perhaps to write any remaining changes to disk that are necessary to finalize
 * the bvfs file before exiting.
 *
 * Return Value
 *   int:  0 if the clean-up process succeeded.
 *        -1 if the clean-up process failed (eg. bv_init was not previously,
 *           called etc.). Also, print a meaningful error to stderr prior to
 *           returning.
 */
int bv_destroy() {
    free_superblock();
    free_file_records();
    close(file_system);
}


// Available Modes for bvfs (see bv_open below)
// Changing these to macros so I can use them in a switch statement as C doesn't support constexpr
#define BV_RDONLY 0
#define BV_WCONCAT 1
#define BV_WTRUNC 2
// int BV_RDONLY = 0;
// int BV_WCONCAT = 1;
// int BV_WTRUNC = 2;


int open_read_only(const char* fileName) {
    // Check if file exists
    int id = file_inode_id(fileName);
    if (id == -1) {
        LOG_ERROR("File %s does not exist", fileName);
    } 

    file_open(id, true);

    return id;
}

int open_writeable(const char* fileName, bool truncate) {
    int id = file_inode_id(fileName);

    if (id != -1 && truncate) {
        // Remove file contents so it is treated as a new file
        file_unlink(id);
        id = -1; // Ensure the inode is created from scratch
    }

    if (id == -1) {
        // If not, create it
        // Get a block to serve as the inode
        // Find an inode that is not in use
        FileRecord* file;
        for (int i = 0; i < 256; ++i) {
            file = files + i;        

            if (file->node->name[0] == '\0') {
                id = i;
                break;
            }
        }
        if (id == -1) {
            LOG_ERROR("Maximum number of files reached\n");
            return -1;
        }
        file->node = (INode*) block_read(id + 1); // Pull in node from disk (should be all 0)
        create_inode(file->node, fileName); // Populate with data
        block_write(file->node, id + 1);
    }


    return file_open(id, false);
}

/*
 * int bv_open(const char *fileName, int mode);
 *
 * This function is intended to open a file in either read or write mode. The
 * above modes identify the method of access to utilize. If the file does not
 * exist, you will create it. The function should return a bvfs file descriptor
 * for the opened file which may be later used with bv_(close/write/read).
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *   mode: The access mode to use for accessing the file
 *           - BV_RDONLY: Read only mode
 *           - BV_WCONCAT: Write only mode, appending to the end of the file
 *           - BV_WTRUNC: Write only mode, replacing the file and writing anew
 *
 * Return Value
 *   int: >=0 Greater-than or equal-to zero value representing the bvfs file
 *           descriptor on success.
 *        -1 if some kind of failure occurred. Also, print a meaningful error to
 *           stderr prior to returning.
 */

int bv_open(const char *fileName, int mode) {
    switch (mode) {
        case BV_RDONLY:
            return open_read_only(fileName);
            break;
        case BV_WCONCAT:
            open_writeable(fileName, false);
            break;
        case BV_WTRUNC:
            open_writeable(fileName, true);
            break;

        default: 
            LOG_ERROR("Invalid mode specified: %d\n", mode);
            return -1;
    }
}

/*
 * int bv_close(int bvfs_FD);
 *
 * This function is intended to close a file that was previously opened via a
 * call to bv_open. This will allow you to perform any finalizing writes needed
 * to the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to fetch
 *             (or create) in the bvfs file system.
 *
 * Return Value
 *   int:  0 if open succeeded.
 *        -1 if some kind of failure occurred (eg. the file was not previously
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_close(int bvfs_FD) {
    FileRecord* file = files + bvfs_FD;
    
    if (file->open == false) {
        LOG_ERROR("Can't close a file that isn't open\n");
        return -1;
    }

    block_write(file->node, bvfs_FD + 1);
    file->open = false;
    return 0;
}

/*
 * int bv_write(int bvfs_FD, const void *buf, size_t count);
 *
 * This function will write count bytes from buf into a location corresponding
 * to the cursor of the file represented by bvfs_FD.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to write to.
 *   buf: The buffer containing the data we wish to write to the file.
 *   count: The number of bytes we intend to write from the buffer to the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes written to the file.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_write(int bvfs_FD, const void *buf, size_t count) {
    return file_write(bvfs_FD, buf, count);
}






/*
 * int bv_read(int bvfs_FD, void *buf, size_t count);
 *
 * This function will read count bytes from the location corresponding to the
 * cursor of the file (represented by bvfs_FD) to buf.
 *
 * Input Parameters
 *   bvfs_FD: The identifier for the file to read from.
 *   buf: The buffer that we will write the data to.
 *   count: The number of bytes we intend to write to the buffer from the file.
 *
 * Return Value
 *   int: >=0 Value representing the number of bytes (read)written to buf.
 *        -1 if some kind of failure occurred (eg. the file is not currently
 *           opened via bv_open). Also, print a meaningful error to stderr
 *           prior to returning.
 */
int bv_read(int bvfs_FD, void *buf, size_t count) {
    file_read(bvfs_FD, buf, count);
}







/*
 * int bv_unlink(const char* fileName);
 *
 * This function is intended to delete a file that has been allocated within
 * the bvfs file system.
 *
 * Input Parameters
 *   fileName: A c-string representing the name of the file you wish to delete
 *             from the bvfs file system.
 *
 * Return Value
 *   int:  0 if the delete succeeded.
 *        -1 if some kind of failure occurred (eg. the file does not exist).
 *           Also, print a meaningful error to stderr prior to returning.
 */
int bv_unlink(const char* fileName) {
    int id = file_inode_id(fileName);
    return file_unlink(id);
}







/*
 * void bv_ls();
 *
 * This function will list the contests of the single-directory file system.
 * First, you must print out a header that declares how many files live within
 * the file system. See the example below in which we print "2 Files" up top.
 * Then display the following information for each file listed:
 *   1) the file size in bytes
 *   2) the number of blocks occupied within bvfs
 *   3) the time and date of last modification (derived from unix timestamp)
 *   4) the name of the file.
 * An example of such output appears below:
 *    | 2 Files
 *    | bytes:  276, blocks: 1, Tue Nov 14 09:01:32 2017, bvfs.h
 *    | bytes: 1998, blocks: 4, Tue Nov 14 10:32:02 2017, notes.txt
 *
 * Hint: #include <time.h>
 * Hint: time_t now = time(NULL); // gets the current unix timestamp (32 bits)
 * Hint: printf("%s\n", ctime(&now));
 *
 * Input Parameters
 *   None
 *
 * Return Value
 *   void
 */
void bv_ls() {
    // Obtain and print the file count
    int file_count = 0;
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;        

        if (strncmp("", file->node->name, MAX_FILE_NAME_LEN) != 0) {
            file_count++;
        }
    }
    printf("%d Files\n", file_count);

    // Print detailed info for each node
    for (int i = 0; i < 256; ++i) {
        FileRecord* file = files + i;        

        // Ignore empty file names
        if (strncmp("", file->node->name, MAX_FILE_NAME_LEN) == 0) {
            continue;
        }

        // Perform calculations needed to display info
        int num_bytes;
        if (file->node->block_count == 0) {
            num_bytes = 0;
        } else {
            num_bytes = (file->node->block_count-1) * BLOCK_SIZE
                        + file->node->block_cursor;
        }

        // Print out the info for this node
        printf("bytes: %d, ", num_bytes);
        printf("blocks: %d, ", file->node->block_count);
        printf("%.24s, ", ctime(&file->node->timestamp));
        printf("%s\n", file->node->name);
    }
}
