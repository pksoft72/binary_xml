#pragma once
#include <sys/stat.h>

off_t FileGetSizeByFd(int fd);

bool WriteToFile(const char *filename,const char *fmt,...);
char *ReadFile(const char *filename,bool MustExist,int *size);

bool Scan(const char *&p,const char *beg);
void SkipSpaces(const char *&p);
bool SkipLine(const char *&p);
bool ScanInt(const char *&p,int &value);
int  GetInt(const char *p);
bool ScanStr(const char *&p,char separator,char *value,unsigned value_size);
bool StrWrite(char *&dest,const char *dest_limit,const char *&src,const char *src_limit);
const char *MakeIdent(const char *Source,const char *style = "");
int GetIdentLen(const char *p);
