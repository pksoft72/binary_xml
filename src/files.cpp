#include "files.h"

#include "macros.h"
#include "strings.h"

#include <string.h>
#include <stdio.h>
#include <alloca.h>
#include <errno.h>
#include <time.h>
#include <dirent.h> // readdir ...

bool force_directory(const char *directory, mode_t mode)
{
    const int len = strlen(directory);
    char *directory_part = static_cast<char*>(alloca(len + 1)); // copy of directory

    for(int i = 1;i < len;i++) // skipping the 1st directory
    {
        if (directory[i] == '/')
        {
            strncpy(directory_part,directory,i);
            directory_part[i] = '\0';
            int err = mkdir(directory_part,mode);
            
//          D(std::cout << "Dbg: " << directory_part << "\n");
    
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

bool scan_dir(const char *base_dir,void *userdata,on_file_event on_file)
{
    DIR *dir = opendir(base_dir);
    if (dir == nullptr)
    {
        ERRNO_SHOW(1101,"opendir",base_dir);
        return false;
    }
    for(;;)
    {
        errno = 0;
        struct dirent *dirent = readdir(dir);
        if (dirent == nullptr) break;

        // typedef bool (*on_file_event)(const char *directory,const char *filename,void *username,uint8_t d_type);
        if (!on_file(base_dir,dirent->d_name,userdata,dirent->d_type)) break;

/*        if (*dirent->d_name == '.') continue; // hidden files .*, current dir and upper dir
        switch (dirent->d_type)
        {
            case DT_DIR:
                {
                    char dir[PATH_MAX];
                    snprintf(dir,sizeof(dir),"%s/%s",base_dir,dirent->d_name);
                    dir[PATH_MAX-1] = '\0';
                    if (!ScanDir(dir)) return false;
                    break;
                }
            case DT_REG:
                {
                    int name_len = strlen(dirent->d_name);
                    if (name_len < 3) continue;
                    if (strcmp(dirent->d_name+name_len-3,".xb") != 0) continue;
                    ASSERT_NO_RET_FALSE(1103,files_count < MAX_XB_FILES);
                    snprintf(files[files_count].filename,PATH_MAX,"%s/%s",base_dir,dirent->d_name);
                    files[files_count].filename[PATH_MAX-1] = '\0';
                    files_count++;
                    break;
                }
            default:
                break;
        }*/
    }
    if (errno != 0)
    {
        ERRNO_SHOW(1102,"readdir",base_dir);
        return false;
    }
    if (closedir(dir) != 0)
    {
        ERRNO_SHOW(2025,"closedir",base_dir);
        return false;
    }    
    return true;
}

