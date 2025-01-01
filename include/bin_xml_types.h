#ifndef _BIN_XML_TYPES_H_
#define _BIN_XML_TYPES_H_


#include <stdint.h>
#include <iostream>

namespace pklib_xml
{

#define MIN_OUT_BUFFER_SIZE 256

//#define AA(_wp) {(_wp) += (4 - (((intptr_t)(_wp) & 3) & 3));}   // align _wp to boundary of 32bits
#define AA(_wp) do {(_wp) += (4 - ((intptr_t)(_wp) & 3)) & 3;} while(0)
#define AA8(_wp) do {(_wp) += (8 - ((intptr_t)(_wp) & 7)) & 7;} while(0)
#define AA16(_wp) do {(_wp) += (16 - ((intptr_t)(_wp) & 15)) & 15;} while(0)
#define CHECK_AA_THIS assert(((intptr_t)this & 3) == 0)

typedef char        hash192_t[24];      // hash - 24B - sha1
typedef char        GUID_t[16];     
typedef uint16_t    IPv6_t[8];      

#define MAX_REETRANT_BUFFERS 64  // workaround for XBT_ToString

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
    XBT_UINT64  = 15,    // stores as binary int64_t - example: 9223372036854775808
    XBT_UNIX_TIME64_MSEC = 16,   // time_t*1000 - 64bit
    XBT_UINT32  = 17,    // stores as binary int64_t
    XBT_UNIX_DATE = 18,  // time() / (24*60*60) -> 32 bit
    XBT_INT32_DECI  = 19,  // int32_t - 1.3 stored as 13
    XBT_INT32_CENTI = 20,  // int32_t - 1.23 stored as 123
    XBT_INT32_MILI  = 21,  // int32_t - 1.234 stored as 1234
    XBT_INT32_MICRO = 22,  // int32_t - 1.234567 stored as 1234567
    XBT_INT32_NANO  = 23,  // int32_t - 1.234567890 stored as 1234567890
    XBT_LAST      = 24,

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

#define XBT_IS_4(type) ((type) == XBT_INT32 || (type) == XBT_INT32_DECI || (type) == XBT_INT32_CENTI || (type) == XBT_INT32_MILI || (type) == XBT_INT32_MICRO || (type) == XBT_INT32_NANO || (type) == XBT_FLOAT || (type) == XBT_UNIX_TIME || (type) == XBT_IPv4 || (type) == XBT_UINT32)
#define XBT_IS_8(type) ((type) == XBT_INT64 || (type) == XBT_DOUBLE || ((type) == XBT_UNIX_TIME64_MSEC) || ((type) == XBT_UINT64))

#define XBT_IS_INT32(type) ((type) == XBT_INT32 || (type) == XBT_INT32_DECI || (type) == XBT_INT32_CENTI || (type) == XBT_INT32_MILI || (type) == XBT_INT32_MICRO || (type) == XBT_INT32_NANO)

#define XBT_IS_VARSIZE(type) ((type) == XBT_BLOB || (type) == XBT_HEX)
#define XBT_FIXEDSIZE(type)  ((type) == XBT_NULL ? 0 : ((type) == XBT_INT64 || (type) == XBT_DOUBLE || (type) == XBT_UINT64 || (type) == XBT_UNIX_TIME64_MSEC ? 8 : ((type) == XBT_GUID ? 16 : ((type) == XBT_SHA1 ? 20 : ((type) == XBT_IPv6 ? 16 : -1)))))

#define XBT2STR(type) ((type) == pklib_xml::XBT_NULL    ? "XBT_NULL" : ((type) == pklib_xml::XBT_VARIANT ? "XBT_VARIANT" : ((type) == pklib_xml::XBT_STRING  ? "XBT_STRING" : ((type) == pklib_xml::XBT_BLOB  ? "XBT_BLOB" : ((type) == pklib_xml::XBT_INT32   ? "XBT_INT32" : ((type) == pklib_xml::XBT_INT64   ? "XBT_INT64" : ((type) == pklib_xml::XBT_FLOAT   ? "XBT_FLOAT" : ((type) == pklib_xml::XBT_DOUBLE  ? "XBT_DOUBLE" : ((type) == pklib_xml::XBT_HEX     ? "XBT_HEX" : ((type) == pklib_xml::XBT_GUID    ? "XBT_GUID" : ((type) == pklib_xml::XBT_SHA1    ? "XBT_SHA1" : ((type) == pklib_xml::XBT_UNIX_TIME   ? "XBT_UNIX_TIME" : ((type) == pklib_xml::XBT_IPv4    ? "XBT_IPv4" : ((type) == pklib_xml::XBT_IPv6    ? "XBT_IPv6" : ((type) == pklib_xml::XBT_UNKNOWN ? "XBT_UNKNOWN" : ((type) == pklib_xml::XBT_UINT64  ? "XBT_UINT64" : ((type) == pklib_xml::XBT_UNIX_TIME64_MSEC ? "XBT_UNIX_TIME64_MSEC" : ((type) == pklib_xml::XBT_UINT32  ? "XBT_UINT32" : ((type) == pklib_xml::XBT_UNIX_DATE ? "XBT_UNIX_DATE" : \
((type) == pklib_xml::XBT_INT32_DECI     ? "XBT_INT32_DECI" :\
((type) == pklib_xml::XBT_INT32_CENTI    ? "XBT_INT32_CENTI" :\
((type) == pklib_xml::XBT_INT32_MILI     ? "XBT_INT32_MILI" :\
((type) == pklib_xml::XBT_INT32_MICRO    ? "XBT_INT32_MICRO" :\
((type) == pklib_xml::XBT_INT32_NANO     ? "XBT_INT32_NANO" :\
((type) == pklib_xml::XBT_LAST    ? "XBT_LAST" :\
 "XBT_???")))))))))))))))))))))))))

struct XML_Binary_Data_Ref
// I have a problem with const char * vs. char * variant of code!
// I need char data for xbw
{
    XML_Binary_Type type;
    char            *data;
    int             size;
    int             content_size; // nominal blob size
};

typedef uint8_t XML_Binary_Type_Stored; // Binary types are stored in array of bytes
extern const char *XML_BINARY_TYPE_NAMES[XBT_LAST+1];

XML_Binary_Type XBT_Detect(const char *value);
XML_Binary_Type XBT_Detect2(const char *value,XML_Binary_Type expected);
XML_Binary_Type XBT_JoinTypes(XML_Binary_Type A,XML_Binary_Type B);

int             XBT_Compare(XML_Binary_Type A_type,const void *A_value,int A_size,
                            XML_Binary_Type B_type,const void *B_value,int B_size);


int             XBT_Size2   (XML_Binary_Type type,int size);
int             XBT_Align   (XML_Binary_Type type);
bool            XBT_Copy    (const char *src,XML_Binary_Type type,int size,char **_wp,char *limit);
bool            XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit);
int             XBT_SizeFromString(const char *src,XML_Binary_Type type);

bool            XBT_Equal   (XML_Binary_Data_Ref A,XML_Binary_Data_Ref B);

const char     *XBT_ToString(XML_Binary_Type type,const char *data);
int             XBT_ToStringChunk(XML_Binary_Type type,const char *data,int &offset,char *dst,int dst_size);
    
void            XBT_ToStringStream(XML_Binary_Type type,const char *data,std::ostream &os);
void            XBT_ToXMLStream(XML_Binary_Type type,const char *data,std::ostream &os);

char*           XBT_Buffer(int size);
void            XBT_Free();

bool            XBT_Test();
} // pklib_xml::
#endif
