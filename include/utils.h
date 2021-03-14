#pragma once
#include <stdint.h>
#include <sys/stat.h>

off_t FileGetSizeByFd(int fd);

bool WriteToFile(const char *filename,const char *fmt,...);
char *ReadFile(const char *filename,bool MustExist,int *size);

// const pointer variants
bool Scan(const char *&p,const char *beg);
bool ScanInt(const char *&p,int &value);
bool ScanInt64(const char *&p,int64_t &value);
bool ScanUInt64(const char *&p,uint64_t &value);
bool ScanStr(const char *&p,char separator,char *value,unsigned value_size);
void SkipSpaces(const char *&p);
bool SkipLine(const char *&p);
int ScanHex(const char *&p,uint8_t *dst,int dst_size);

// writeable pointer variants
bool ScanW(char *&p,const char *beg);
bool ScanIntW(char *&p,int &value);
bool ScanInt64W(char *&p,int64_t &value);
bool ScanUInt64W(char *&p,uint64_t &value);
bool ScanStrW(char *&p,char separator,char *value,unsigned value_size);
void SkipSpacesW(char *&p);
bool SkipLineW(char *&p);

char *AllocFilenameChangeExt(const char *filename,const char *extension);

bool EatEnd(char *p,const char *end);
int  GetInt(const char *p);
bool StrWrite(char *&dest,const char *dest_limit,const char *&src,const char *src_limit);
const char *MakeIdent(const char *Source,const char *style = "");
int GetIdentLen(const char *p);


/* This function will encode binary data into base64 encoded string */
char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size);
/* This function will decode binary data into base64 encoded string */
int base64_decode(const unsigned char *src,int src_size,char *dst,int dst_size);
