#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <sys/stat.h>

extern const char HEX[16+1];

typedef char char40_t[40];  // YYYYMMDD HHMMSS.sss

off_t FileGetSize(const char *filename);
off_t FileGetSizeByFd(int fd);

bool WriteToFile(const char *filename,const char *fmt,...);
char *ReadFile(const char *filename,bool must_exist,int *size);
char *ShareFileRO(const char *filename,int *fd,int max_pool_size);
char *ShareFileRW(const char *filename,bool must_exist,int *fd,int max_pool_size);
void CloseSharedFile(char *pool,int max_pool_size,int fd,const char *debug_info);

// const pointer variants
bool Scan(const char *&p,const char *beg);
bool ScanInt(const char *&p,int &value);
bool ScanInt64(const char *&p,int64_t &value);
bool ScanUInt64(const char *&p,uint64_t &value);
bool ScanUInt64Hex(const char *&p,uint64_t &value);
bool ScanUnixTime(const char *&p,uint32_t &value);
bool ScanUnixTime64msec(const char *&p,int64_t &value);
bool ScanUnixDate(const char *&p,int32_t &value);
char *UnixDate2Str(int32_t value,char *dst);
char *UnixTime642Str(int64_t value,char *dst);
bool ScanStr(const char *&p,char separator,char *value,unsigned value_size);
void SkipSpaces(const char *&p);
bool SkipLine(const char *&p);
bool Go(const char *&p,char separator);

int ScanHex(const char *&p,uint8_t *dst,int dst_size);
const char *Hex2Str(const char *src,int src_size,char *dst);
const char *Hex3Str(const char *src,int src_size,char *dst);

// writeable pointer variants
bool ScanW(char *&p,const char *beg);
bool ScanIntW(char *&p,int &value);
bool ScanInt64W(char *&p,int64_t &value);
bool ScanUInt64W(char *&p,uint64_t &value);
bool ScanStrW(char *&p,char separator,char *value,unsigned value_size);
void SkipSpacesW(char *&p);
bool SkipLineW(char *&p);

int64_t GetCurrentTime64();


char *AllocFilenameChangeExt(const char *filename,const char *extension,const char *target_dir = nullptr);

bool EatEnd(char *p,const char *end);
int  GetInt(const char *p);
uint32_t GetVersion(const char *p); 
bool StrWrite(char *&dest,const char *dest_limit,const char *&src,const char *src_limit);
const char *MakeIdent(const char *Source,const char *style = "");
int GetIdentLen(const char *p);

bool streq(const char *src0,const char *src1);

const char *Int64ToStr(int64_t value,char40_t buffer);

/* This function will encode binary data into base64 encoded string */
char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size);
/* This function will decode binary data into base64 encoded string */
int base64_decode(const unsigned char *src,int src_size,char *dst,int dst_size);
#endif
