#ifndef FILES_H_
#define FILES_H_

#include <sys/stat.h>
#include <sys/types.h>

bool force_directory(const char *directory, mode_t mode);

// TODO: int is not very good for this?
off_t file_getsize(const char *filename);
time_t file_gettime(const char *filename);

#endif
