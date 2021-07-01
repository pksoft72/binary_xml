#include "bin_xml_types.h"
#include "macros.h"
#include "utils.h"
#include <assert.h>
#include "bin_xml.h"

namespace pklib_xml
{

const char *XML_BINARY_TYPE_NAMES[XBT_LAST+1] = {
    "NULL",
    "VARIANT",
    "STRING",
    "BLOB",
    "INT32",
    "INT64",
    "FLOAT",
    "DOUBLE",
    "HEX",
    "GUID",
    "SHA1",
    "UNIX_TIME",
    "IPv4",
    "IPv6",
    "Unknown??",
    "UINT64",
    "UNIX_TIME64_MSEC",
    "UINT32",
    "UNIX_DATE",
    "!!!!"
};

XML_Binary_Type XBT_Detect(const char *value)
{
    if (value == nullptr) return XBT_NULL;
    if (*value == '\0') return XBT_NULL;
    int str_len = strlen(value);
    int digits = 0;
    int hexadigits = 0;
    int negative = 0;
    int dots = 0;
    int dashes = 0;
    int colons = 0;
    int others = 0;
    int exponent = 0;
    int spaces = 0;
    int exponent_pos = -1;
    bool overflow64 = false;
    
    int len = 0;
    uint64_t X = 0,X0 = 0,EX = 0;
    for(const char *p = value;*p != '\0';p++)
    {
        if (*p == '-')
            if (len == exponent_pos+1) negative++;
            else dashes++;
        else if (*p >= '0' && *p <= '9')
        {
            X = X*10 + (*p-'0');
            if (X < X0) overflow64 = true;
            X0 = X;
            EX = EX*10 + (*p-'0');
            digits++;
        }
        else if (*p >= 'a' && *p <= 'f')
            hexadigits++;
        else if (*p >= 'A' && *p <= 'F')
        {
            hexadigits++;
            if (*p == 'E')
            {
                exponent_pos = len;
                EX = 0;
                exponent++;
            }
        }
        else if (*p == '.')
            dots++;
        else if (*p == ':')
            colons++;
        else if (*p == ' ')
            spaces++;
        else
            others++;
    }
    if (str_len == 10 && digits == 8 && value[4] == '-' && value[7] == '-')
        return XBT_UNIX_DATE;
    if (digits > 0 && digits <= 19 && hexadigits == 0 && negative <= 1 
        && dots == 0 && dashes == 0 && colons == 0 && others == 0 && exponent == 0 && !overflow64)
    {
        if (negative == 0 && X <= 0x7fffffff) return XBT_INT32;
        if (negative == 1 && X <= 0x80000000) return XBT_INT32;
        if (negative == 0 && (X >> 32) == 0) return XBT_UINT32;
        if (negative == 0 && X > 0x7fffffffffffffff) return XBT_UINT64;
        return XBT_INT64;
    }
    if (digits > 0 && hexadigits-exponent == 0 && negative <= 2
        && dots <= 1 && dashes == 0 && colons == 0 && others == 0 && exponent <= 1)
    {
        if (EX > 38 || digits >= 8) return XBT_DOUBLE;  
        else return XBT_FLOAT;
    }
    if (digits+hexadigits == 40 && negative == 0 && dots == 0 && dashes == 0 && colons == 0 && others == 0)
    {
//        std::cout << ANSI_BLUE_DARK "XBT_SHA1 detected in value " << value << ANSI_RESET_LF;
        return XBT_SHA1;
    }
// 2021-06-15 22:41:40
    if (str_len == 19 && digits == 14 && spaces == 1 && negative == 2 && colons == 2 &&
        value[4] == '-' && value[7] == '-' && value[13] == ':' && value[16] == ':')
        return XBT_UNIX_TIME;
// DISABLED - not fully supported yet
//  if (digits+hexadigits > 0 && negative == 0 && dots == 0 && dashes == 0 && colons == 0 && others == 0) 
//      return XBT_HEX;

    return XBT_STRING;
}

XML_Binary_Type XBT_JoinTypes(XML_Binary_Type A,XML_Binary_Type B)
{
    if (A == XBT_VARIANT) return A; // non compatible types
    if (A == XBT_NULL) return B;
    if (A == XBT_LAST) return B; // not used yet
    if (A == XBT_UNKNOWN) return B;
    
    if (B == XBT_VARIANT) return B; // non compatible types
    if (B == XBT_NULL) return A;
    if (B == XBT_LAST) return A; // not used yet
    if (B == XBT_UNKNOWN) return A;

    if (B == A) return B;

    if ((B == XBT_INT32 || B == XBT_INT64 || B == XBT_FLOAT || B == XBT_DOUBLE) &&
        (A == XBT_INT32 || A == XBT_INT64 || A == XBT_FLOAT || A == XBT_DOUBLE))
    {
        unsigned det_mask = ((B == XBT_FLOAT || B == XBT_DOUBLE) ? 1 : 0) |   // je to floating point?
                            ((B == XBT_INT64 || B == XBT_DOUBLE) ? 2 : 0);    // je to velký typ?
        unsigned exp_mask = ((A == XBT_FLOAT || A == XBT_DOUBLE) ? 1 : 0) |   // je to floating point?
                            ((A == XBT_INT64 || A == XBT_DOUBLE) ? 2 : 0);    // je to velký typ?
        const XML_Binary_Type number_types[] = {XBT_INT32,XBT_FLOAT,XBT_INT64,XBT_DOUBLE};
        return number_types[det_mask | exp_mask];
    }
    //std::cout << ANSI_MAGENTA_DARK "Join types " << XML_BINARY_TYPE_NAMES[A] << " + " << XML_BINARY_TYPE_NAMES[B] << " --> VARIANT" ANSI_RESET_LF;
    return XBT_VARIANT;
    
}

XML_Binary_Type XBT_Detect2(const char *value,XML_Binary_Type expected)
{
    if (expected == XBT_VARIANT) return expected; // non compatible types
    XML_Binary_Type detected = XBT_Detect(value);
    return XBT_JoinTypes(detected,expected);
}

int XBT_Compare(XML_Binary_Type A_type,const void *A_value,int A_size,XML_Binary_Type B_type,const void *B_value,int B_size)
{
#define RETURN_COMPARE_OF_TYPE(_T) \
    if (A_size != sizeof(_T) || B_size != sizeof(_T)) return INT_NULL_VALUE;\
    if (*reinterpret_cast<const _T *>(A_value) < *reinterpret_cast<const _T *>(B_value)) return -1;\
    if (*reinterpret_cast<const _T *>(A_value) > *reinterpret_cast<const _T *>(B_value)) return 1;\
    return 0;

    if (A_type == B_type) // the same type
    {
        switch (A_type)
        {
            case XBT_INT32:     RETURN_COMPARE_OF_TYPE(int32_t);
            case XBT_UNIX_DATE: RETURN_COMPARE_OF_TYPE(int32_t);
            case XBT_UINT32:    RETURN_COMPARE_OF_TYPE(uint32_t);
            case XBT_UNIX_TIME: RETURN_COMPARE_OF_TYPE(uint32_t);
            case XBT_UNIX_TIME64_MSEC: RETURN_COMPARE_OF_TYPE(int64_t); 
            case XBT_INT64:     RETURN_COMPARE_OF_TYPE(int64_t);
            case XBT_UINT64:    RETURN_COMPARE_OF_TYPE(uint64_t);
            case XBT_FLOAT:     RETURN_COMPARE_OF_TYPE(float);
            case XBT_DOUBLE:    RETURN_COMPARE_OF_TYPE(double);

            case XBT_STRING:
                                return strcmp(reinterpret_cast<const char *>(A_value),reinterpret_cast<const char *>(B_value));
            case XBT_HEX:
            case XBT_GUID:
            case XBT_SHA1:
            case XBT_IPv4:
            case XBT_IPv6:
                                if (A_size == B_size) // the same size
                                    return memcmp(A_value,B_value,A_size);
            default:
                                return INT_NULL_VALUE;
        }
    }
    if (A_size == B_size) // the same size
        return memcmp(A_value,B_value,A_size);
    // TODO: some conversions could be done here, but it is too much of code - matrix of types and their conversions
    return INT_NULL_VALUE; // uncomparable
#undef RETURN_COMPARE_OF_TYPE
}

int XBT_Size(XML_Binary_Type type,int size)
// This function will return total size of type, when I need to store size bytes of information.
{
    switch(type)
    {
        case XBT_NULL:
        case XBT_UNKNOWN:
        case XBT_LAST:
            ASSERT_NO_RET_N1(1068,size == 0);
            return 0;
        case XBT_VARIANT:
            ASSERT_NO_RET_N1(1069,type != XBT_VARIANT); // must be known
            break;
        case XBT_STRING:
            return ++size; // terminating character
        case XBT_BLOB:
            return size + sizeof(int32_t);
        case XBT_INT32:
            ASSERT_NO_RET_N1(1070,size == 0);
            return sizeof(int32_t);
        case XBT_UINT32:
            ASSERT_NO_RET_N1(1185,size == 0);
            return sizeof(uint32_t);
        case XBT_INT64:
            ASSERT_NO_RET_N1(1071,size == 0);
            return sizeof(int64_t);
        case XBT_UINT64:
            ASSERT_NO_RET_N1(1171,size == 0);
            return sizeof(uint64_t);
        case XBT_FLOAT:
            ASSERT_NO_RET_N1(1072,size == 0);
            return sizeof(float);
        case XBT_DOUBLE:
            ASSERT_NO_RET_N1(1073,size == 0);
            return sizeof(double);
        case XBT_HEX:
            return size + sizeof(int32_t);
        case XBT_GUID:
            ASSERT_NO_RET_N1(1074,size == 0);
            return 16;
        case XBT_SHA1:
            ASSERT_NO_RET_N1(1075,size == 0);
            return 24;
        case XBT_UNIX_TIME:
            ASSERT_NO_RET_N1(1076,size == 0);
            return sizeof(uint32_t);
        case XBT_UNIX_DATE:
            ASSERT_NO_RET_N1(0,size == 0);
            return sizeof(int32_t);
        case XBT_IPv4:
            ASSERT_NO_RET_N1(1077,size == 0);
            return 4;
        case XBT_IPv6:
            ASSERT_NO_RET_N1(1078,size == 0);
            return 16;
        case XBT_UNIX_TIME64_MSEC:
            ASSERT_NO_RET_N1(1172,size == 0);
            return sizeof(uint64_t);
    }
    return 0;
}

int XBT_Align(XML_Binary_Type type)
{
    switch(type)
    {
        case XBT_NULL:
        case XBT_UNKNOWN:
        case XBT_LAST:
            return 0;
        case XBT_VARIANT:
            return 0; // TODO: alignment depend on transported type
        case XBT_STRING:
            return 0;
        case XBT_BLOB:
        case XBT_HEX:
        case XBT_INT32:
        case XBT_UINT32:
        case XBT_FLOAT:
        case XBT_GUID:
        case XBT_SHA1:
        case XBT_UNIX_TIME:
        case XBT_UNIX_DATE:
        case XBT_IPv4:
            return 4;
        case XBT_UINT64:
        case XBT_INT64:
        case XBT_DOUBLE:
        case XBT_IPv6:
        case XBT_UNIX_TIME64_MSEC:
            return 8;
    }
    return 0;
}

bool XBT_Copy(const char *src,XML_Binary_Type type,int size,char **_wp,char *limit)
{
    int value_size = XBT_Size(type,size);
    int align_size = XBT_Align(type);
    if (limit - *_wp < value_size + align_size) 
    {
        LOG(ANSI_RED_BRIGHT "XBT_Copy: type=%s no space (%d) for value (%d+%d)\n",XML_BINARY_TYPE_NAMES[type],(int)(limit - *_wp),(int)value_size,(int)align_size);
        return false; // cannot fit into destination
    }
    if (type == XBT_STRING)
        strcpy(*_wp,src);
    else if (align_size == 4)
    {
        AA(*_wp);
        memcpy(*_wp,src,value_size);
/*        if (type == XBT_BLOB)
            LOG("Copy-BLOB: size=%d align=%d and value size=%d\n",size,align_size,value_size);
        else if (type == XBT_UINT32)
            LOG("Copy-UInt32: value=%u",*reinterpret_cast<const uint32_t*>(src));
*/
    }
    else if (align_size == 8)
    {
        AA8(*_wp);
        memcpy(*_wp,src,value_size);
/*        if (type == XBT_UNIX_TIME64_MSEC)
            LOG("Copy-int64-time: value=%08x-%08x size=%d --> %p",*reinterpret_cast<const uint32_t*>(src),*reinterpret_cast<const uint32_t*>(src+4),value_size,*_wp);
*/
    }
    *_wp += value_size;
    return true; // OK
}

bool XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit)
{
    switch (type)
    {
        case XBT_NULL:
            if (strlen(src) == 0) return true;
            fprintf(stderr,"%s: Conversion of value '%s' to binary representation is not defined!" ANSI_RESET_LF,XML_BINARY_TYPE_NAMES[type],src);
            break;
            
        case XBT_INT32:
            AA(*_wp);
            *reinterpret_cast<int32_t*>(*_wp) = atoi(src);
            *_wp += sizeof(int32_t);
            return true;
        case XBT_INT64:
            {
                int64_t v;
                ASSERT_NO_RET_FALSE(0,ScanInt64(src,v));
                AA8(*_wp);
                *reinterpret_cast<int64_t*>(*_wp) = v;
                *_wp += sizeof(int64_t);
                return true;
            }
            
        case XBT_STRING:
            strcpy(*_wp,src);
            *_wp += strlen(src)+1;
            return true;
        case XBT_SHA1:
            {
                AA(*_wp);
                int scanned = ScanHex(src,reinterpret_cast<uint8_t *>(*_wp),20);
                *_wp += scanned;
                return scanned == 20;
            }
        case XBT_UNIX_TIME:
            {
                AA(*_wp);
                uint32_t tm0;
                if (!ScanUnixTime(src,tm0))
                {
                    fprintf(stderr,"UNIX_TIME: Conversion of value '%s' to binary representation failed!" ANSI_RESET_LF,src);
                    return false;
                }
                *reinterpret_cast<uint32_t*>(*_wp) = tm0;
                *_wp += sizeof(uint32_t);
                return true;
            }
        case XBT_UNIX_DATE:
            {
                AA(*_wp);
                int32_t dt0;
                if (!ScanUnixDate(src,dt0))
                {
                    fprintf(stderr,"UNIX_DATE: Conversion of value '%s' to binary representation failed!" ANSI_RESET_LF,src);
                    return false;
                }
                *reinterpret_cast<int32_t*>(*_wp) = dt0;
                *_wp += sizeof(int32_t);
                return true;
            }
        default:
            fprintf(stderr,"%s: Conversion of value '%s' to binary representation is not defined!" ANSI_RESET_LF,XML_BINARY_TYPE_NAMES[type],src);
//            assert(false);
            return false;
    // TODO: support all types!
    }
    return false;
}

const char *XBT_ToString(XML_Binary_Type type,const char *data)
{
    int align_size = XBT_Align(type);
    if (align_size == 4)
        AA(data);
    else if (align_size == 8)
        AA8(data);

    static char buffers[MAX_REETRANT_BUFFERS][256+1];
    static int buffer_idx = 0;

    switch (type)
    {
        case XBT_NULL: 
            return nullptr; // empty value
        case XBT_STRING: 
            return reinterpret_cast<const char *>(data); 
        case XBT_INT32:
        case XBT_UINT32: 
        case XBT_INT64:
        case XBT_UINT64:
        case XBT_FLOAT:
        case XBT_UNIX_TIME:
        case XBT_UNIX_DATE:
        case XBT_UNIX_TIME64_MSEC:
        case XBT_SHA1:
        case XBT_GUID:
            {
                
                int offset = 0;
                XBT_ToStringChunk(type,data,offset,buffers[buffer_idx],sizeof(buffers[0]));
                const char *ret = buffers[buffer_idx++];
                if (buffer_idx >= MAX_REETRANT_BUFFERS) buffer_idx = 0;
                return ret;
            }
        case XBT_BLOB:
            {
                uint32_t size = *reinterpret_cast<const uint32_t*>(data);   
                if (size == 0) return nullptr;

                const unsigned char *src = reinterpret_cast<const unsigned char*>(data)+4;   

                // char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size)
                int dst_size = size/3 * 4 + 5;
                char *dst = XBT_Buffer(dst_size);
                ASSERT_NO_RET_NULL(1205,dst != nullptr);
                return base64_encode(src,size,dst,dst_size);
            }
        default:
            LOG_ERROR("Unsupported type %d",type);
            return nullptr;
    }
    
    return nullptr;
}

int XBT_ToStringChunk(XML_Binary_Type type,const char *data,int &offset,char *dst,int dst_size)
{
    ASSERT_NO_RET_N1(1203,dst != nullptr);
    ASSERT_NO_RET_N1(1204,dst_size > MIN_OUT_BUFFER_SIZE); // small objects will not be chunked

    int align = XBT_Align(type);
    if (align == 4)
    {
        AA(data);
    }
    else if (align == 8)
    {
        AA8(data);
    }
    

    dst[--dst_size] = '\0';
    switch(type)
    {
        case XBT_NULL: 
            return 0; // empty value
        case XBT_STRING: 
            {
                const char *p = reinterpret_cast<const char *>(data);
                int len = strlen(p);
                len -= offset;
                p += offset;
                if (len <= 0) return 0; // finished
                strncpy(dst,p,dst_size);
                int out = strlen(dst);
                offset += out;
                return out;
            }
        case XBT_INT32: 
            if (offset > 0) return 0;
            offset += sizeof(int32_t);

            snprintf(dst,dst_size,"%d",*reinterpret_cast<const int32_t*>(data));
            return strlen(dst);
        case XBT_UINT32: 
            if (offset > 0) return 0;
            offset += sizeof(uint32_t);

            snprintf(dst,dst_size,"%u",*reinterpret_cast<const uint32_t*>(data));
            return strlen(dst);

        case XBT_INT64:
            {
                if (offset > 0) return 0;
                offset += sizeof(int64_t);

                int64_t value = *reinterpret_cast<const int64_t*>(data);   
                if (value == 0) 
                {
                    strcpy(dst,"0");
                    return 1;
                }
                if (value == -576460752303423488) 
                {
                    strcpy(dst,"9223372036854775808");
                    return strlen(dst);
                }
                //if (value == -0x8000000000000000) return "9223372036854775808";
                char *d = dst+dst_size;
                *(--d) = '\0';
                bool neg = (value < 0);
                if (neg) value = -value;
                while (value > 0)
                {
                    *(--d) = '0'+(value % 10);
                    value /= 10;
                }
                if (neg) *(--d) = '-';
                int len = (dst+dst_size) - d - 1;
                memmove(dst,d,len);
                return len;
            }
        case XBT_UINT64:
            {
                if (offset > 0) return 0;
                offset += sizeof(uint64_t);

                uint64_t value = *reinterpret_cast<const uint64_t*>(data);   
                if (value == 0) 
                {
                    strcpy(dst,"0");
                    return 1;
                }
                char *d = dst+dst_size;
                *(--d) = '\0';
                while (value > 0)
                {
                    *(--d) = '0'+(value % 10);
                    value /= 10;
                }
                int len = (dst+dst_size) - d - 1;
                memmove(dst,d,len);
                return len;
            }
        case XBT_FLOAT:
            if (offset > 0) return 0;
            offset += sizeof(float);

            snprintf(dst,dst_size,"%f",*reinterpret_cast<const float*>(data));
            return strlen(dst);
        case XBT_UNIX_TIME:
            {
                if (offset > 0) return 0;
                offset += sizeof(uint32_t);

                uint32_t value = *reinterpret_cast<const uint32_t*>(data);
                time_t tm0 = (time_t)value;
                struct tm tm1;
                memset(dst,0,dst_size);

                gmtime_r(&tm0,&tm1);
                snprintf(dst,dst_size,"%04d-%02d-%02d %02d:%02d:%02d",1900+tm1.tm_year,1+tm1.tm_mon,tm1.tm_mday,tm1.tm_hour,tm1.tm_min,tm1.tm_sec);
                return strlen(dst);
            }
        case XBT_UNIX_DATE:
            {
                if (offset > 0) return 0;
                offset += sizeof(int32_t);

                int32_t value = *reinterpret_cast<const int32_t*>(data);
                if (UnixDate2Str(value,dst) == nullptr) return 0; // FAIL
                return strlen(dst);
            }
        case XBT_UNIX_TIME64_MSEC:
            {
                if (offset > 0) return 0;
                offset += sizeof(int64_t);

//                LOG("TIME64(%p) --> %08X:%08X",data, *reinterpret_cast<const uint32_t*>(data), *(reinterpret_cast<const uint32_t*>(data)+1));

                int64_t value = *reinterpret_cast<const int64_t*>(data);
                time_t tm0 = (time_t)(value/1000);
                struct tm tm1;
                memset(dst,0,dst_size);

                gmtime_r(&tm0,&tm1);
                snprintf(dst,dst_size,"%04d-%02d-%02d %2d:%02d:%02d.%03d",1900+tm1.tm_year,1+tm1.tm_mon,tm1.tm_mday,tm1.tm_hour,tm1.tm_min,tm1.tm_sec,(int)(value % 1000));
                return strlen(dst);
            }
        case XBT_BLOB:
            {
                uint32_t size = *reinterpret_cast<const uint32_t*>(data);   
                const unsigned char *src = reinterpret_cast<const unsigned char*>(data)+4;   
                
                if (offset >= (int)size) 
                {
                    dst[0] = '\0';
                    return 0;
                }
                int max_chunk = ((dst_size-1) >> 2)*3;
                int left = size - offset;
                int chunk = (left > max_chunk) ? max_chunk : left;
                
                base64_encode(src+offset,chunk,dst,dst_size);
                // char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size)

                offset += chunk;
                return strlen(dst);
            }
        case XBT_SHA1:
            {
                if (offset >= 20) return 0;
                Hex2Str(data+offset,20-offset,dst);
                int ret = 2*(20-offset);
                offset = 20;
                return ret;
            }
        default:
            assert(false);
            return 0;
    }
}

void XBT_ToStringStream(XML_Binary_Type type,const char *data,std::ostream &os)
{
    char buffer[4096+1];
    int offset = 0;
    for(;;)
    {
        // int XBT_ToStringChunk(XML_Binary_Type type,const char *data,int &offset,char *dst,int dst_size)
        int len = XBT_ToStringChunk(type,data,offset,buffer,sizeof(buffer));
        if (len == 0) break;
        buffer[len] = '\0';
        os << buffer;
    }
}

void XBT_ToXMLStream(XML_Binary_Type type,const char *data,std::ostream &os)
{
    char buffer[4096+1];
    int offset = 0;
    for(;;)
    {
        // int XBT_ToStringChunk(XML_Binary_Type type,const char *data,int &offset,char *dst,int dst_size)
        int len = XBT_ToStringChunk(type,data,offset,buffer,sizeof(buffer));
        if (len == 0) break;
        for(int i = 0;i < len;i++)
        {
            if (buffer[i] == '<') os << "&lt;";
            else if (buffer[i] == '>') os << "&gt;";
            else if (buffer[i] == '&') os << "&amp;";
            else if (buffer[i] == '\"') os << "&quot;";
            else os << buffer[i];
        }
    }
}

bool XBT_Equal(XML_Binary_Data_Ref A,XML_Binary_Data_Ref B)
{
    if (A.type != B.type)
    {
        if (A.type == XBT_STRING)
        {
            char *buffer = reinterpret_cast<char*>(alloca(A.size+64));
            char *buffer_limit = buffer+A.size+64;
            char *p = buffer;
            if (!XBT_FromString(A.data,B.type,&p,buffer_limit)) return false; // cannot convert

            A.data = buffer;
            A.size = p - buffer;
        }
        else if (B.type == XBT_STRING)
        {
            char *buffer = reinterpret_cast<char*>(alloca(B.size+64));
            char *buffer_limit = buffer+B.size+64;
            char *p = buffer;
            if (!XBT_FromString(B.data,A.type,&p,buffer_limit)) return false; // cannot convert

            B.data = buffer;
            B.size = p - buffer;
        }
        else
            return false; // no compare of any types
    }
    if (A.size != B.size) return false;
    return memcmp(A.data,B.data,A.size) == 0;
}

char *g_output_buffer = nullptr;
int   g_output_buffer_size = 0;
char* XBT_Buffer(int size)
{
    if (size <= g_output_buffer_size) return g_output_buffer;
    XBT_Free();
    g_output_buffer_size = ((size >> 12) << 12) + 8192;
    g_output_buffer = new char[g_output_buffer_size];
    if (g_output_buffer == nullptr) g_output_buffer_size = 0;
    return g_output_buffer;
}

void XBT_Free()
{
    if (g_output_buffer == nullptr) return;
    delete [] g_output_buffer;
    g_output_buffer = nullptr;
    g_output_buffer_size = 0;
}

//-------------------------------------------------------------------------------------------------

static int XBT_TestType(XML_Binary_Type type,const char *src,const char *hex = nullptr)
{
    int failures = 0;
    XML_Binary_Type type_detected = XBT_Detect(src);
    if (type != type_detected)
        std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T1 " ANSI_RED_BRIGHT "Value \"" << src << "\" was detected as " << XML_BINARY_TYPE_NAMES[type_detected] << ANSI_RESET_LF;

    int src_size = strlen(src);

    const int buffer_len = src_size*2;
    char buffer[buffer_len]; // -std=c++1y 
    char *p = buffer;
    char *p_limit = buffer + buffer_len;

    
    if (!XBT_FromString(src,type,&p,p_limit))
    {
        std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T2 " ANSI_RED_BRIGHT "Cannot convert value \"" << src << "\" to binary representation." << ANSI_RESET_LF;
        failures++;
    }
    
    int dst_size = p - buffer;
    char hex_image[dst_size*2+1];

    for(int i = 0;i < dst_size;i++)   
    {
        uint8_t V = buffer[i];
        hex_image[i*2] = HEX[V >> 4];
        hex_image[i*2+1] = HEX[V & 0xf];
    }
    hex_image[dst_size*2] = '\0';

    if (hex != nullptr)
    {
        if (strcmp(hex,hex_image) != 0)
        {
            std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T3 " ANSI_RED_BRIGHT "Unexpected image " << hex_image << "\n should be " ANSI_GREEN_BRIGHT << hex  << ANSI_RESET_LF;
            failures++;
        }
    }
    else
        std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T4 " ANSI_MAGENTA_BRIGHT "Please check image of " << src << " --(" << dst_size << ")--> " << hex_image << ANSI_RESET_LF;

    
    int offset = 0;
    int str_offset = 0;
    for(;;)
    {
        char str_buffer[4096+1];
        MEMSET(str_buffer,0);

        int size = XBT_ToStringChunk(type,buffer,offset,str_buffer,sizeof(str_buffer)-1);
        if (size <= 0) break;

        if (size + str_offset > src_size)
        {
            std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T5 " ANSI_RED_BRIGHT "Source " << src << " with length " << src_size << " is converted to length " << (size + str_offset) << "!!!"  ANSI_RESET_LF;
            failures++;
        }        
        else if (memcmp(src+str_offset,str_buffer,size) != 0)
        {
            std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T6 " ANSI_RED_BRIGHT "Conversion of " << src+str_offset << " failed with part " << str_buffer << " at offset " << str_offset << "!!!"  ANSI_RESET_LF;
            failures++;
        }
        
        str_offset += size;
    }
    if (str_offset != src_size)
    {
        std::cerr << ANSI_WHITE_HIGH << XML_BINARY_TYPE_NAMES[type] << ": T7 " ANSI_RED_BRIGHT "Conversion of " << src+str_offset 
            << " results to length " << str_offset << " and not not " << src_size << "!!!"  ANSI_RESET_LF;
        failures++;
    }

    return failures;
}

bool XBT_Test()
{
    bool ok;
    ok = XBT_TestType(XBT_STRING,"Petr Kundrata","50657472204b756e647261746100") && ok;
    ok = XBT_TestType(XBT_NULL,"","") && ok;

    ok = XBT_TestType(XBT_INT32,"125","7d000000") && ok;
    ok = XBT_TestType(XBT_INT32,"2147483647","ffffff7f") && ok;
    ok = XBT_TestType(XBT_INT32,"-2147483648","00000080") && ok;
   
    ok = XBT_TestType(XBT_INT64,"-2147483648000","000000000cfeffff") && ok;
    

    ok = XBT_TestType(XBT_SHA1,"9b6aa6cc7c5a71c13fc7ee1011ef309c333a904c","9b6aa6cc7c5a71c13fc7ee1011ef309c333a904c") && ok;

    ok = XBT_TestType(XBT_UNIX_TIME,"1970-01-01 00:00:00", "00000000") && ok;
    ok = XBT_TestType(XBT_UNIX_TIME,"2021-06-15 22:41:40", "a42cc960") && ok;
    
    ok = XBT_TestType(XBT_UNIX_DATE,"1970-01-01", "00000000") && ok;
    ok = XBT_TestType(XBT_UNIX_DATE,"1969-12-31", "ffffffff") && ok;
    ok = XBT_TestType(XBT_UNIX_DATE,"2000-01-01", "cd2a0000") && ok; // 10957
    
    return ok;
}

}
