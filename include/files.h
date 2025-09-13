#ifndef FILES_H_
#define FILES_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>

bool force_directory(const char *directory, mode_t mode);

// TODO: int is not very good for this?
off_t file_getsize(const char *filename);
time_t file_gettime(const char *filename);
bool file_exists(const char *filename);

typedef bool (*on_file_event)(const char *directory,const char *filename,void *userdata,uint8_t d_type);
/*

              DT_BLK      This is a block device.
              DT_CHR      This is a character device.
              DT_DIR      This is a directory.
              DT_FIFO     This is a named pipe (FIFO).
              DT_LNK      This is a symbolic link.
              DT_REG      This is a regular file.
              DT_SOCK     This is a UNIX domain socket.
              DT_UNKNOWN  The file type could not be determined.
*/
#define D_TYPE2STR(t) ((t) == DT_BLK ? "BLK" : ((t) == DT_CHR ? "CHR" : ((t) == DT_DIR ? "DIR" : ((t) == DT_FIFO ? "FIFO" : ((t) == DT_LNK ? "LNK" : ((t) == DT_REG ? "REG" : ((t) == DT_SOCK ? "SOCK" : ((t) == DT_UNKNOWN ? "UNKNOWN" : "???")))))))) 



bool scan_dir(const char *directory,void *userdata,on_file_event on_file);

#endif
