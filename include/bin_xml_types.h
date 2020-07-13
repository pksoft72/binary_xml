#pragma once

#include <stdint.h>
#include <iostream>

namespace pklib_xml
{

#define MIN_OUT_BUFFER_SIZE 256

//#define AA(_wp) {(_wp) += (4 - (((intptr_t)(_wp) & 3) & 3));}   // align _wp to boundary of 32bits
#define AA(_wp) do {(_wp) += (4 - ((intptr_t)(_wp) & 3)) & 3;} while(0)
#define AA8(_wp) do {(_wp) += (8 - ((intptr_t)(_wp) & 7)) & 7;} while(0)
#define CHECK_AA_THIS assert(((intptr_t)this & 3) == 0)

typedef char        hash192_t[24];      // hash - 24B - sha1
typedef char        GUID_t[16];     
typedef uint16_t    IPv6_t[8];      

enum XML_Binary_Type 
{
    XBT_NULL    = 0,    // empty value is almost compatible with all values
    XBT_VARIANT = 1,    // stores with XML_Binary_Type in 1st byte
    XBT_STRING  = 2,    // string 0 terminated
    XBT_BLOB  = 3,    // stored as uint32_t length + data
    XBT_INT32   = 4,    // stores as binary int32_t
    XBT_INT64   = 5,    // stores as binary int64_t
    XBT_FLOAT   = 6,    // stores as 32bit float
    XBT_DOUBLE  = 7,    // stores as 64bit double
    XBT_HEX     = 8,    // only hexadecimal values
    XBT_GUID    = 9,    // GUID
    XBT_SHA1    = 10,   // sha1 - 20 bytes
    XBT_UNIX_TIME   = 11,   // time_t - 32bit
    XBT_IPv4    = 12,   // 192.168.3.15
    XBT_IPv6    = 13,   // TODO: not implemented yet
    XBT_UNKNOWN = 14,   // type detection failed
    XBT_UINT64  = 15,    // stores as binary int64_t
    XBT_UNIX_TIME64_MSEC = 16,   // time_t*1000 - 64bit
    XBT_UINT32  = 17,    // stores as binary int64_t
    XBT_LAST    = 18,

#if 0
// Array types are not supported now
// Encoded as BLOB of basic value
    XBT_INT8    = 18,
    XBT_UINT8   = 19,
    XBT_INT16   = 20,
    XBT_UINT16  = 21
    XBT_ARRAY   = 127   // when used this bitmask, data consist of many values of base type
#endif
};

#define XBT_IS_VARSIZE(type) ((type) == XBT_BLOB || (type) == XBT_HEX)

typedef uint8_t XML_Binary_Type_Stored; // Binary types are stored in array of bytes
extern const char *XML_BINARY_TYPE_NAMES[XBT_LAST];

XML_Binary_Type XBT_Detect(const char *value);
XML_Binary_Type XBT_Detect2(const char *value,XML_Binary_Type expected);
XML_Binary_Type XBT_JoinTypes(XML_Binary_Type A,XML_Binary_Type B);

int             XBT_Compare(XML_Binary_Type A_type,const void *A_value,int A_size,
                            XML_Binary_Type B_type,const void *B_value,int B_size);


int             XBT_Size    (XML_Binary_Type type,int size);
int             XBT_Align   (XML_Binary_Type type);
bool            XBT_Copy    (const char *src,XML_Binary_Type type,int size,char **_wp,char *limit);
bool            XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit);

const char     *XBT_ToString(XML_Binary_Type type,const char *data);
int             XBT_ToStringChunk(XML_Binary_Type type,const char *data,int &offset,char *dst,int dst_size);
    
void            XBT_ToStringStream(XML_Binary_Type type,const char *data,std::ostream &os);
void            XBT_ToXMLStream(XML_Binary_Type type,const char *data,std::ostream &os);

char*           XBT_Buffer(int size);
void            XBT_Free();


} // pklib_xml::
