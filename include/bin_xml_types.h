#pragma once

#include <stdint.h>

namespace pklib_xml
{

typedef char        hash192_t[24];      // hash - 24B - sha1
typedef char        GUID_t[16];     
typedef uint16_t    IPv6_t[8];      

enum XML_Binary_Type 
{
    XBT_NULL    = 0,    // empty value is almost compatible with all values
    XBT_VARIABLE    = 1,    // stores with XML_Binary_Type in 1st byte
    XBT_STRING  = 2,    // string 0 terminated
    XBT_BINARY  = 3,    // stored as uint32_t length + data
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
    XBT_VARIANT = 14,   // mixing many types
    XBT_UNKNOWN = 15,   // type detection failed
    XBT_LAST    = 16
};

typedef uint8_t XML_Binary_Type_Stored; // Binary types are stored in array of bytes
extern const char *XML_BINARY_TYPE_NAMES[XBT_LAST];
} // pklib_xml::
