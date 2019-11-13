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
 
void file_create(const char* name, int size) {
   FILE* fd = fopen(name, "wb");
   fseek(fd, size-1, SEEK_SET);
   fputc(0, fd);
   fclose(fd);
}
 
#endif /* UTIL_H */
