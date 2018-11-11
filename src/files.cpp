#include "files.h"

#include "macros.h"
#include "strings.h"

#include <string.h>
#include <stdio.h>
#include <alloca.h>
#include <errno.h>
#include <time.h>

bool force_directory(const char *directory, mode_t mode)
{
	const int len = strlen(directory);
	char *directory_part = static_cast<char*>(alloca(len + 1));	// copy of directory

	for(int i = 1;i < len;i++) // skipping the 1st directory
	{
		if (directory[i] == '/')
		{
			strncpy(directory_part,directory,i);
			directory_part[i] = '\0';
			int err = mkdir(directory_part,mode);
			
//			D(std::cout << "Dbg: " << directory_part << "\n");
	
			if (err == 0) continue;// ok - created
			if (errno == EEXIST) continue;// ok - already exists
		
			ERRNO_SHOW(1009,"mkdir",directory_part);
			return false;
		}
		
	}
	return true;
}
// I want to ensure, that directory is exists.

off_t file_getsize(const char *filename)
{
	struct stat file_info;
	int err = stat(filename,&file_info);
	if (err != 0)
	{
		if (errno == ENOENT)
			return -1; // file not exists - no message

		ERRNO_SHOW(1010,"stat",filename);	
		return -1; // failed
	}
	return file_info.st_size;
}

time_t file_gettime(const char *filename)
{
	struct stat file_info;
	int err = stat(filename,&file_info);
	if (err != 0)
	{
		if (errno == ENOENT)
			return -1; // file not exists - no message

		ERRNO_SHOW(1011,"stat",filename);	
		return -1; // failed
	}
	return file_info.st_mtim.tv_sec;
}

