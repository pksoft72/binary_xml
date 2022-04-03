#ifndef FILES_H_
#define FILES_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

bool force_directory(const char *directory, mode_t mode);

// TODO: int is not very good for this?
off_t file_getsize(const char *filename);
time_t file_gettime(const char *filename);

typedef bool (*on_file_event)(const char *directory,const char *filename,void *userdata,uint8_t d_type);

bool scan_dir(const char *directory,void *userdata,on_file_event on_file);

#endif
