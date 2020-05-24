#include "utils.h"
#include "macros.h"
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>


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

char *ReadFile(const char *filename,bool MustExist,int *size)
// will read file and allocate enough memory for file store in memory
{
    int fd = open(filename,O_RDONLY);
    if (fd == -1)
    {
//      if (!MustExist && errno == ) return nullptr;
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

const char *AllocFilenameChangeExt(const char *filename,const char *extension)
{
    if (filename == nullptr) return nullptr; // it is correct to send empty filename
    if (extension == nullptr)
    {
        char *new_name = new char[strlen(filename)+1];
        strcpy(new_name,filename);
        return new_name;
    }
    const char *dot = strrchr(filename,'.');
    int len = (dot != nullptr ? dot - filename : strlen(filename));
    int ext_len = strlen(extension);

    char *new_name = new char[len+ext_len+1];
    strncpy(new_name,filename,len);
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
    while (isspace(*p)) p++;
    
    bool neg = (*p == '-');
    if (neg) p++;
    if (!isdigit(*p)) return INT_NULL_VALUE;

    int x = 0;
    while (isdigit(*p))
        x = x *10 + (*(p++) - '0');
    
    return neg ? -x : x;
    
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
        else if (isspace(*Source))
        {
            if (space_to_underscore)
            {
                *(d++) = '_';
                len++;
            }
            else if (space_to_uppercase)
            {
                need_uppercase = true;
            }
            while (isspace(*Source)) Source++;
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



