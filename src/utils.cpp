#include "utils.h"
#include "macros.h"
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h> // clock_gettime()
#include <limits.h>
#include <sys/mman.h>   // mmap


const char HEX[16+1] = "0123456789abcdef";
const char HEX_UP[16+1] = "0123456789ABCDEF";

bool WriteToFile(const char *filename,const char *fmt,...)
{
    FILE *f = fopen(filename,"w");
    if (f == nullptr)
    {
        ERRNO_SHOW(1007,"WriteToFile-fopen",filename);
        return false;
    }
    va_list ap;
    va_start(ap,fmt);
    
    int ok = vfprintf(f,fmt,ap);
    if (ok < 0)
    {
        ERRNO_SHOW(1008,"WriteToFile-fprintf",fmt);
        return false;
    }

    va_end(ap);

    if (fclose(f) == EOF)
    {
        ERRNO_SHOW(1009,"WriteToFile-fclose",filename);
        return false;
    }
    return true;
}

char *ReadFile(const char *filename,bool must_exist,int *size)
// will read file and allocate enough memory for file store in memory
{
    int fd = open(filename,O_RDONLY);
    if (fd == -1)
    {
//      if (!must_exist && errno == ) return nullptr;
        ERRNO_SHOW(1010,"ReadFile-open",filename);
        return nullptr; 
    }
    off_t file_size = FileGetSizeByFd(fd);
    char *data = nullptr;
    if (file_size > 0)
    {
        data = new char[file_size+1];
        ssize_t bytes_read = read(fd,data,file_size);
        if (bytes_read < 0)
        {
            ERRNO_SHOW(1011,"ReadFile-read",filename);
            delete [] data;
            data = nullptr;
        }
        else if (bytes_read < file_size)
        {
            std::cerr << ANSI_RED_BRIGHT << "ReadFile-read(" << filename << ") cannot read whole file (" 
                << bytes_read << " < " << file_size << ")" ANSI_RESET_LF;
            delete [] data;
            data = nullptr;
        }
        else
            data[file_size] = '\0';

    }

    int ok_close = close(fd);   
    if (ok_close == -1)
    {
        ERRNO_SHOW(1012,"ReadFile-close",filename);
    }

    if (size != nullptr)
        *size = file_size;
    return data;    
}

//-------------------------------------------------------------------------------------------------

static char *s_ShareFile(const char *filename,int open_flags,int *fd,int max_pool_size);

char *ShareFileRO(const char *filename,int *fd,int max_pool_size)
{
    return s_ShareFile(filename,O_RDONLY | O_NOATIME,fd,max_pool_size);    
}

char *ShareFileRW(const char *filename,bool must_exist,int *fd,int max_pool_size)
{
    int open_flags = O_RDWR | O_NOATIME; 
    if (!must_exist) open_flags |= O_CREAT;
    return s_ShareFile(filename,open_flags,fd,max_pool_size);    
}

static char *s_ShareFile(const char *filename,int open_flags,int *fd,int max_pool_size)
{
    int open_fd = open(filename,open_flags,S_IRUSR | S_IWUSR | S_IRGRP);
    if (open_fd < 0)
    {
        ERRNO_SHOW(2062,"open",filename);
        return nullptr;
    }
    //  void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
    int prot = PROT_READ;
    if ((open_flags & O_RDWR) == O_RDWR)
        prot |= PROT_WRITE;
    char *pool = reinterpret_cast<char *>(mmap(nullptr,max_pool_size,prot, 
                MAP_SHARED,  //  Share  this  mapping.   Updates to the mapping are visible to other processes mapping the same region, 
                // and (in the case of file-backed mappings) are carried through to the underlying file.  
                // (To precisely control when updates are carried through to the underlying file requires the use of msync(2).)

                open_fd, 0));
    if (pool == MAP_FAILED)
    {
        ERRNO_SHOW(2063,"mmap",filename);
        close(open_fd);
        return nullptr;
    }


    if (fd != nullptr)  
        *fd = open_fd;
    else
        close(open_fd);
    return pool;
}

void CloseSharedFile(char *pool,int max_pool_size,int fd,const char *debug_info)
{
    if (fd > 0)
    {
        if (close(fd) < 0)
            ERRNO_SHOW(2064,"close",debug_info);
    }
    if (pool != nullptr)
    {
        if (munmap(pool,max_pool_size) != 0)
            ERRNO_SHOW(1949,"munmap",debug_info);
    }
}

off_t FileGetSize(const char *filename)
{
    struct stat file_info;
    int err = stat(filename,&file_info);
    if (err != 0)
    {
        if (errno == ENOENT)
            return -1; // file not exists - no message

        ERRNO_SHOW(0,"fstat",filename); 
        return -1; // failed
    }
    if (S_ISDIR(file_info.st_mode)) 
    {
        std::cerr << "\n" ANSI_RED_BRIGHT "Error " ANSI_RED_DARK " while " 
            ANSI_RED_BRIGHT "FileGetSize(" << filename << ")" ANSI_RED_DARK " 'file' is directory!" ANSI_RESET;
        return -1; // directory
    }
    return file_info.st_size;
}

off_t FileGetSizeByFd(int fd)
{
    struct stat file_info;
    int err = fstat(fd,&file_info);
    if (err != 0)
    {
        if (errno == ENOENT)
            return -1; // file not exists - no message

        ERRNO_SHOW(1013,"fstat","FileGetSizeByFd"); 
        return -1; // failed
    }
    if (S_ISDIR(file_info.st_mode)) 
    {
        std::cerr << "\n" ANSI_RED_BRIGHT "Error " ANSI_RED_DARK " while " 
            ANSI_RED_BRIGHT "FileGetSizeByFd" ANSI_RED_DARK " 'file' is directory!" ANSI_RESET;
        return -1; // directory
    }
    return file_info.st_size;
}

int64_t GetCurrentTime64()
{
    struct timespec tm0;
    int ok = clock_gettime(CLOCK_REALTIME,&tm0);
    if (ok < 0) return (int64_t)1000 * time(nullptr);

    return (int64_t)tm0.tv_sec*1000 +
           (int64_t)tm0.tv_nsec / 1000000;
}

char *AllocFilenameChangeExt(const char *filename,const char *extension,const char *target_dir)
// I have filename - example            src/common/test.xml
// optional new extension - example     .xb
// optional target directory - example  build/temp
// result:                              build/temp/src/common/test.xb
// want to process some shortcuts       build/test/.. --> build
// but these shortcuts cannot go out of target_dir
{
    if (filename == nullptr) return nullptr; // it is correct to send empty filename

// removing ./ prefixes
    while (filename[0] == '.' && filename[1] == '/') 
        filename += 2;

    char filepath[PATH_MAX] = "";
    filepath[PATH_MAX-1] = '\0'; // terminator

    if (target_dir != nullptr)
    {
        // removing / root reference
        if (filename[0] == '/') 
            filename++; 

        STRCPY(filepath,target_dir);
        int len = strlen(filepath);
        filepath[len++] = '/';
        strncpy(filepath+len,filename,PATH_MAX-len-1);
    }
    else 
        STRCPY(filepath,filename);

    if (extension == nullptr)
    {
        char *new_name = new char[strlen(filepath)+1];
        strcpy(new_name,filepath);
        return new_name;
    }
    const char *dot = strrchr(filepath,'.');
    int len = (dot != nullptr ? dot - filepath : strlen(filepath));
    int ext_len = strlen(extension);

    char *new_name = new char[len+ext_len+1];
    strncpy(new_name,filepath,len);
    strcpy(new_name+len,extension);

    return new_name;
}

bool EatEnd(char *p,const char *end)
{
    int len0 = strlen(p);
    int len1 = strlen(end);
    if (len1 > len0) return false; // too short
    if (strcmp(p + len0 - len1,end) != 0) return false; // not exactly end
    *(p + len0 - len1) = '\0'; // truncate
    return true;
}

//-------------------------------------------------------------------------------------------------

bool Scan(const char *&p,const char *beg)
{
    while (isspace(*p)) p++;

    const char *p1 = p;
    while (*p1 == *beg && *beg != 0)
    {
        p1++;
        beg++;
    }
    if (*beg != 0) return false; // not equal
    p = p1;
    return true;
}

bool ScanInt(const char *&p,int &value)
{
    while (isspace(*p)) p++;

    const char *p1 = p;
    bool neg = (*p1 == '-');
    if (neg) p1++;
    if (!isdigit(*p1)) return false;

    int x = 0;
    while (isdigit(*p1))
        x = x *10 + (*(p1++) - '0');
    
    p = p1;
    value = (neg ? -x : x);
    return true;
}

bool ScanInt64(const char *&p,int64_t &value)
{
    while (isspace(*p)) p++;

    const char *p1 = p;
    bool neg = (*p1 == '-');
    if (neg) p1++;
    if (!isdigit(*p1)) return false;

    int64_t x = 0;
    while (isdigit(*p1))
        x = x *10 + (*(p1++) - '0');
    
    p = p1;
    value = (neg ? -x : x);
    return true;
}

bool ScanUInt64(const char *&p,uint64_t &value)
{
    while (isspace(*p)) p++;

    uint64_t x = 0;
    const char *p1 = p;
    if (*p1 == '0' && *(p1+1) == 'x') // HEXADECIMAL !
    {
        p1 += 2;
        if (!isxdigit(*p1)) return false;
        while (*p1 != '\0')
        {
            if (*p1 >= '0' && *p1 <= '9')
                x = (x << 4) + (*(p1++) - '0');
            else if (*p1 >= 'A' && *p1 <= 'F')
                x = (x << 4) + (*(p1++) - 'A' + 10);
            else if (*p1 >= 'a' && *p1 <= 'f')
                x = (x << 4) + (*(p1++) - 'a' + 10);
            else break;
        }
    }
    else
    {
        if (!isdigit(*p1)) return false;

        while (isdigit(*p1))
            x = x *10 + (*(p1++) - '0');
    }
    
    p = p1;
    value = x;
    return true;
}

bool ScanUInt64Hex(const char *&p,uint64_t &value)
{
    while (isspace(*p)) p++;

    uint64_t x = 0;
    const char *p1 = p;
    if (!isxdigit(*p1)) return false;
    while (*p1 != '\0')
    {
        if (*p1 >= '0' && *p1 <= '9')
            x = (x << 4) + (*(p1++) - '0');
        else if (*p1 >= 'A' && *p1 <= 'F')
            x = (x << 4) + (*(p1++) - 'A' + 10);
        else if (*p1 >= 'a' && *p1 <= 'f')
            x = (x << 4) + (*(p1++) - 'a' + 10);
        else break;
    }

    p = p1;
    value = x;
    return true;
}

const int MONTH_OFFSET[12] = {
    0,
    31,
    31+28,
    31+28+31,
    31+28+31+30,
    31+28+31+30+31,
    31+28+31+30+31+30,
    31+28+31+30+31+30+31,
    31+28+31+30+31+30+31+31,
    31+28+31+30+31+30+31+31+30,
    31+28+31+30+31+30+31+31+30+31,
    31+28+31+30+31+30+31+31+30+31+30,
    //    31+28+31+30+31+30+31+31+30+31+30+31,
};

const int LEAP_MONTH_OFFSET[12] = {
    0,
    31,
    31+29,
    31+29+31,
    31+29+31+30,
    31+29+31+30+31,
    31+29+31+30+31+30,
    31+29+31+30+31+30+31,
    31+29+31+30+31+30+31+31,
    31+29+31+30+31+30+31+31+30,
    31+29+31+30+31+30+31+31+30+31,
    31+29+31+30+31+30+31+31+30+31+30,
    //    31+29+31+30+31+30+31+31+30+31+30+31,
};

const int MAX_DAY = 0xffffffff/(24*60*60);

bool ScanUnixTime(const char *&p,uint32_t &value)
{
    value = 0;
    int year,month,day,hour,minute,second;

    const char *beg = p;

    if (!ScanInt(p,year)) return false;    
    if (*(p++) != '-') return false;
    if (!ScanInt(p,month)) return false;    
    if (month < 1 || month > 12) return false; // I dont know months out of range
    if (*(p++) != '-') return false;
    if (!ScanInt(p,day)) return false;    

    if (year < 1970) 
    { 
        fprintf(stderr,"Unsupported year %d! Unix epoch starts at 1970.",year);
        return false;
    }

    value = (year - 1970) * 365;
    value += (year - 1968 - 1) / 4;   // -1 don't add 1 day for this year now
    value -= (year - 1900 - 1) / 100;
    value += (year - 1600 - 1) / 400;

    bool is_leap_year = (((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0);
    if (is_leap_year)   
        value += LEAP_MONTH_OFFSET[month-1];
    else
        value += MONTH_OFFSET[month-1];
    value += day - 1;

    if (value > MAX_DAY)
    {
        fprintf(stderr,"Date %s exceed 32 bit unsigned int! Please use different type.",beg);
        return false;
    }

    value *= 24*60*60;

    if (*p == '\0') return true; // end of string
    if (*(p++) != ' ') return false;
    
    if (!ScanInt(p,hour)) return false;    
    value += hour*3600;
    if (*p == '\0') return true; // end of string
    if (*(p++) != ':') return false;

    if (!ScanInt(p,minute)) return false;    
    value += minute*60;
    if (*(p++) != ':') return false;

    if (!ScanInt(p,second)) return false;    
    value += second;
    return true;
}

bool ScanUnixTime64msec(const char *&p,int64_t &value)
{
    value = 0;
    uint32_t tm32;
    if (!ScanUnixTime(p,tm32)) return false;
    if (*(p++) != '.') return false;

    if (*p < '0' || *p > '9') return false;
    int v000 = 100 * (*(p++) - '0');
    if (*p < '0' || *p > '9') return false;
    v000 += 10 * (*(p++) - '0');
    if (*p < '0' || *p > '9') return false;
    v000 += (*(p++) - '0');

    value = v000 + (int64_t)tm32 * 1000;
    return true;
}

bool ScanUnixDate(const char *&p,int32_t &value)
{
    int year,month,day;

    //const char *beg = p;

    if (!ScanInt(p,year)) return false;    
    if (*(p++) != '-') return false;
    if (!ScanInt(p,month)) return false;    
    if (month < 1 || month > 12) return false; // I dont know months out of range
    if (*(p++) != '-') return false;
    if (!ScanInt(p,day)) return false;    

    if (year < -365749 || year > 369689) 
    { 
        fprintf(stderr,"Unsupported year %d! Only dates in range -365749 .. 369689 is allowed.",year);
        return false;
    }

    value = (year - 1970) * 365;
    value += (year - 1968 - 1) / 4;   // -1 don't add 1 day for this year now
    value -= (year - 1900 - 1) / 100;
    value += (year - 1600 - 1) / 400;

    bool is_leap_year = (((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0);
    if (is_leap_year)   
        value += LEAP_MONTH_OFFSET[month-1];
    else
        value += MONTH_OFFSET[month-1];
    value += day - 1;

    return true;
}

#define YEARS400_TO_DAYS (400 * 365 + 100 - 4 + 1) // 400 year + 100 leap days each 4th day - 4 leap day per century + 1 leap day per 400
#define YEARS100_TO_DAYS (100 * 365 + 25 - 1) // 100 years + 100 leap days each 4th day - 4 leap day per century + 1 leap day per 400
#define YEARS4_TO_DAYS (4*365 + 1)
#define YEAR_TO_DAYS 365

const uint8_t DAY2MONTH[365] = {
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12};
const uint8_t DAY2MONTH_LEAP[366] = {
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12};

char *UnixDate2Str(int32_t value,char *dst)
{
    value -= 10957; // 1.1.1970 base to 1.1.2000
    int year = 2000;

    int y400 = value / YEARS400_TO_DAYS;
    if (value < 0) y400--; // 1999 -> 1600..1999
    value -= y400 * YEARS400_TO_DAYS;


    year += y400*400;

    // I have value in range 0 .. YEARS400_TO_DAYS - 1
    // year 0 is leap
    bool is_leap;
    if (value < 366)    // 0..365 = 366 days
        is_leap = true;
    else
    {
        value--; // as if it is not leap
        int y100 = value / YEARS100_TO_DAYS;
        value -= y100 * YEARS100_TO_DAYS;
        
        year += y100 * 100;
        // value is 0..YEARS100_TO_DAYS-1
        if (value < 365)
            is_leap = false; // first year is not leap
        else
        {
            value++; // add as if 100 was leap year
            int y4 = value / YEARS4_TO_DAYS;
            value -= y4 * YEARS4_TO_DAYS;

            year += y4 * 4;
            if (value < 366) // 1st of it is leap
                is_leap = true;
            else
            {
                value--;
                int y1 = value / YEAR_TO_DAYS;
                value -= y1 * YEAR_TO_DAYS;
                year += y1;
                is_leap = false;
            }
            
        }
    }
    int month,day;
    if (is_leap)
    {
        ASSERT_NO_RET_NULL(2023,value >= 0 && value < 365);
        month = DAY2MONTH_LEAP[value];
        day = 1+value - LEAP_MONTH_OFFSET[month-1];
    }
    else
    {
        ASSERT_NO_RET_NULL(2024,value >= 0 && value < 366);
        month = DAY2MONTH[value];
        day = 1+value - MONTH_OFFSET[month-1];
    }
    sprintf(dst,"%04d-%02d-%02d",year,month,day);

    return dst;
}

bool ScanStr(const char *&p,char separator,char *value,unsigned value_size)
{
    while (isspace(*p)) p++;

    char *d = value;
    if (separator == ' ') // bere cokoliv, co je isspace
    {
        while (*p != '\0' && !isspace(*p))
        {
            if (value_size > 1)
            {
                *(d++) = *p;
                value_size--;
            }
            p++;
        }
        if (*p != '\0') p++; // sezere oddelovac!
    }
    else
    {
        while (*p != '\0' && *p != separator)
        {
            if (value_size > 1)
            {
                *(d++) = *p;
                value_size--;
            }
            p++;
        }
        if (*p == separator) p++; // sezere oddelovac!
    }
    *d = '\0';
    return *value != '\0'; // string is not empty
}

void SkipSpaces(const char *&p)
{
    while (isspace(*p)) p++;
}

bool SkipLine(const char *&p)
{
    while (*p != '\0' && *p != '\n') p++;
    if (*p == '\0') return false; // end of file
    p++;    // skip \n
    return true;
}

bool Go(const char *&p,char separator)
{
    while (*p != '\0')
        if (*(p++) == separator) return true;
    return false;    
}

int ScanHex(const char *&p,uint8_t *dst,int dst_size)
{
    memset(dst,0,dst_size);
    if (*p == '\0') return false;
    for(int count = 0;count < dst_size;count++)
    {
        if (p[0] == '-') p++;
        else if (p[0] == ' ') p++;

        int value = 0;
        if (p[0] >= '0' && p[0] <= '9')
            value = p[0] - '0';
        else if (p[0] >= 'a' && p[0] <= 'f')
            value = p[0] - 'a' + 10;
        else if (p[0] >= 'A' && p[0] <= 'F')
            value = p[0] - 'A' + 10;
        else return count;

        value <<= 4;

        if (p[1] >= '0' && p[1] <= '9')
            value |= p[1] - '0';
        else if (p[1] >= 'a' && p[1] <= 'f')
            value |= p[1] - 'a' + 10;
        else if (p[1] >= 'A' && p[1] <= 'F')
            value |= p[1] - 'A' + 10;
        else return count;

        p += 2;

        *(dst++) = value;
    }
    return dst_size;
}

const char *Hex2Str(const char *src,int src_size,char *dst)
{
    if (src_size < 0) src_size = 0; // don't write to negative indexes!
    for(int i = 0;i < src_size;i++)
    {
        uint8_t value = static_cast<uint8_t>(src[i]);
        dst[(i << 1)]   = HEX[value >> 4];
        dst[(i << 1)+1] = HEX[value & 0xf];
    }
    dst[src_size << 1] = '\0';
    return dst;
}

const char *Hex3Str(const char *src,int src_size,char *dst)
{
    if (src_size < 0) src_size = 0; // don't write to negative indexes!
    char *d = dst;
    for(int i = 0;i < src_size;i++)
    {
        uint8_t value = static_cast<uint8_t>(src[i]);
        *(d++) = HEX_UP[value >> 4];
        *(d++) = HEX_UP[value & 0xf];
        *(d++) = ' ';
    }
    *(d++) = '\0';
    return dst;
}

//-------------------------------------------------------------------------------------------------

bool ScanW(char *&p,const char *beg)
{
    while (isspace(*p)) p++;

    char *p1 = p;
    while (*p1 == *beg && *beg != 0)
    {
        p1++;
        beg++;
    }
    if (*beg != 0) return false; // not equal
    p = p1;
    return true;
}

bool ScanIntW(char *&p,int &value)
{
    while (isspace(*p)) p++;

    char *p1 = p;
    bool neg = (*p1 == '-');
    if (neg) p1++;
    if (!isdigit(*p1)) return false;

    int x = 0;
    while (isdigit(*p1))
        x = x *10 + (*(p1++) - '0');
    
    p = p1;
    value = (neg ? -x : x);
    return true;
}

bool ScanInt64W(char *&p,int64_t &value)
{
    while (isspace(*p)) p++;

    char *p1 = p;
    bool neg = (*p1 == '-');
    if (neg) p1++;
    if (!isdigit(*p1)) return false;

    int64_t x = 0;
    while (isdigit(*p1))
        x = x *10 + (*(p1++) - '0');
    
    p = p1;
    value = (neg ? -x : x);
    return true;
}

bool ScanUInt64W(char *&p,uint64_t &value)
{
    while (isspace(*p)) p++;

    uint64_t x = 0;
    char *p1 = p;
    if (*p1 == '0' && *(p1+1) == 'x') // HEXADECIMAL !
    {
        p1 += 2;
        if (!isxdigit(*p1)) return false;
        while (*p1 != '\0')
        {
            if (*p1 >= '0' && *p1 <= '9')
                x = (x << 4) + (*(p1++) - '0');
            else if (*p1 >= 'A' && *p1 <= 'F')
                x = (x << 4) + (*(p1++) - 'A' + 10);
            else if (*p1 >= 'a' && *p1 <= 'f')
                x = (x << 4) + (*(p1++) - 'a' + 10);
            else break;
        }
    }
    else
    {
        if (!isdigit(*p1)) return false;

        while (isdigit(*p1))
            x = x *10 + (*(p1++) - '0');
    }
    
    p = p1;
    value = x;
    return true;
}

bool ScanStrW(char *&p,char separator,char *value,unsigned value_size)
{
    while (isspace(*p)) p++;

    char *d = value;
    if (separator == ' ') // bere cokoliv, co je isspace
    {
        while (*p != '\0' && !isspace(*p))
        {
            if (value_size > 1)
            {
                *(d++) = *p;
                value_size--;
            }
            p++;
        }
    }
    else
    {
        while (*p != '\0' && *p != separator)
        {
            if (value_size > 1)
            {
                *(d++) = *p;
                value_size--;
            }
            p++;
        }
    }
    if (*p != '\0') p++; // sezere oddelovac!
    *d = '\0';
    return *value != '\0'; // string is not empty
}

void SkipSpacesW(char *&p)
{
    while (isspace(*p)) p++;
}

bool SkipLineW(char *&p)
{
    while (*p != '\0' && *p != '\n') p++;
    if (*p == '\0') return false; // end of file
    p++;    // skip \n
    return true;
}

//-------------------------------------------------------------------------------------------------

int  GetInt(const char *p)
{
    if (p == nullptr) return INT_NULL_VALUE;

    while (isspace(*p)) p++;
    
    bool neg = (*p == '-');
    if (neg) p++;
    if (!isdigit(*p)) return INT_NULL_VALUE;

    int x = 0;
    while (isdigit(*p))
        x = x *10 + (*(p++) - '0');
    
    return neg ? -x : x;
    
}

uint32_t GetVersion(const char *p)
{
    if (p == nullptr) return 0;

    uint32_t version = 0;
    for(int i = 0;*p != '\0' && i < 4;i++)
    {
        int V;
        if (!ScanInt(&p,V)) break;
        LIMITE(V,0,255);
        version |= (V << ((3-i) * 8)); // 24,16,8,0
        if (*p != '.') break;
        p++;
    }
    return version;
}


bool StrWrite(char *&dest,const char *dest_limit,const char *&src,const char *src_limit)
{
    int dst_space = dest_limit - dest;
    int src_size = (src_limit != nullptr ? src_limit - src : strlen(src));
    if (src_size > dst_space) return false; // nic neudělám a končím
    strncpy(dest,src,src_size);
    dest += src_size;
    src = src_limit;
    return true;
}


const char *MakeIdent(const char *Source,const char *style)
// non reetrant!!!
{
    static char _MakeIdent_Ident[64];
    char *d = _MakeIdent_Ident;
    int len = 0;
// number at the beginning
    if (*Source >= '0' && *Source <= '9')
    {
        *(d++) = '_';
        *(d++) = *(Source++);
        len += 2;
    }

    bool space_to_underscore = strchr(style,'_');
    bool space_to_uppercase = strchr(style,'U');
    bool all_uppercase = strstr(style,"U!") != nullptr;
    bool need_uppercase = space_to_uppercase;

    while (len < (int)sizeof(_MakeIdent_Ident)-1 && *Source != '\0')
    {
        if ((*Source >= 'a' && *Source <= 'z') ||
            (*Source >= 'A' && *Source <= 'Z') ||
            (len > 0 && *Source >= '0' && *Source <= '9') ||
            *Source == '_')
        {
            if (need_uppercase || all_uppercase)
            {
                *(d++) = toupper(*(Source++));
                need_uppercase = false;
            }
            else
                *(d++) = *(Source++);
            len++;
        }
        else if (isspace(*Source) || *Source == '-')
        {
            if (space_to_underscore || *Source == '-')
            {
                *(d++) = '_';
                len++;
            }
            else if (space_to_uppercase)
            {
                need_uppercase = true;
            }
            while (isspace(*Source) || *Source == '-') Source++;
        }
        else
            Source++; // bad char for id
    }
    *d = '\0';
    return _MakeIdent_Ident;
}

int GetIdentLen(const char *p)
{
    int len = 0;
    if (!isalpha(*p) && *p != '_') return 0;
    len++;p++;
    while(isalnum(*p) || *p == '_')
    {
        len++;p++;
    }
    return len; 
}

bool streq(const char *src0,const char *src1) 
{
    if (src0 == nullptr)
    {
        if (src1 == nullptr) return true; // the same
        if (*src1 == '\0') return true;
        return false;
    }
    if (src1 == nullptr)
    {
        if (*src0 == '\0') return true;
        return false;
    }
    return strcmp(src0,src1) == 0;
}
//-------------------------------------------------------------------------------------------------
static const char* BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/* This function will encode binary data into base64 encoded string */
char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size)
// inspired by toBase64() in btppl_veiltext.c
// https://cs.wikipedia.org/wiki/Base64
{

    char *dst_base = dst;

    while (src_size > 0 && dst_size >= 2)
    {
        int A = src[0];
        int B = src_size > 1 ? src[1] : 0;
        int C = src_size > 2 ? src[2] : 0;
        src += 3;

        *(dst++) = BASE64[A >> 2];
        *(dst++) = BASE64[((A & 3) << 4) | (B >> 4)];
        dst_size -= 2;
        if (src_size == 1 || dst_size == 0) break; // no B
        *(dst++) = BASE64[((B & 15) << 2) | (C >> 6)];
        dst_size--;
        if (src_size == 2 || dst_size == 0) break; // no C
        *(dst++) = BASE64[C & 63];
        dst_size--;

        src_size -= 3;
    }
    for(int c = (dst-dst_base) & 3;c > 0 && dst_size > 0 && c < 4;c++,dst_size--)
            *(dst++) = '=';
    if (dst_size > 1)
        *dst = 0;
    return dst_base;
}

static const int8_t BACK64[256] = {
/*  0*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 16*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/* 32*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
/* 48*/52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
/* 64*/-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
/* 80*/15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 
/* 96*/-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
/*112*/41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, 
/*128*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*144*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*160*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*176*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*192*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*208*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*224*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
/*240*/-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/* This function will decode binary data into base64 encoded string */
int base64_decode(const unsigned char *src,int src_size,char *dst,int dst_size)
{
// init output array
    memset(dst,0,dst_size);

    char *d = dst;
    while(src_size >= 2 && dst_size > 0)
    {
        int K = BACK64[src[0]];                                 // 0..63  K: AAAAAA
        int L = BACK64[src[1]];                                 // 0..63  L: AABBBB
        int M = (src_size > 2 ? BACK64[src[2]] : -1);           // 0..63  M: BBBBCC
        int N = (src_size > 3 ? BACK64[src[3]] : -1);           // 0..63  N: CCCCCC

        if (K < 0) break;
        if (L < 0) break;
        /*A*/ *(d++) = (K << 2) | (L >> 4);
        if (M < 0) break;
        if (dst_size <= 1) break;
        /*B*/ *(d++) = (L & 0xf) << 4 | (M >> 2);
        if (N < 0) break;
        if (dst_size <= 2) break;
        /*C*/ *(d++) = (M & 3) << 6 | N;

        src         += 4;
        src_size    -= 4;

        dst_size    -= 3;
    }
    return d-dst;
}



