#include "bin_xml_types.h"
#include "macros.h"
#include "utils.h"
#include <assert.h>

namespace pklib_xml
{

XML_Binary_Type XBT_Detect(const char *value)
{
    if (value == nullptr) return XBT_NULL;
    if (*value == '\0') return XBT_NULL;
    int digits = 0;
    int hexadigits = 0;
    int negative = 0;
    int dots = 0;
    int dashes = 0;
    int colons = 0;
    int others = 0;
    int exponent = 0;
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
        else
            others++;
    }
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
        return XBT_SHA1;
    if (digits+hexadigits > 0 && negative == 0 && dots == 0 && dashes == 0 && colons == 0 && others == 0)
        return XBT_HEX;

    return XBT_STRING;
}

XML_Binary_Type XBT_JoinTypes(XML_Binary_Type A,XML_Binary_Type B)
{
    if (A == XBT_VARIANT) return A; // non compatible types
    if (A == XBT_NULL) return B;
    if (A == XBT_LAST) return B; // not used yet
    
    if (B == XBT_VARIANT) return B; // non compatible types
    if (B == XBT_NULL) return A;
    if (B == XBT_LAST) return A; // not used yet

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
            case XBT_UINT32:    RETURN_COMPARE_OF_TYPE(uint32_t);
            case XBT_UNIX_TIME: RETURN_COMPARE_OF_TYPE(uint32_t);
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

const char *XBT_ToString(XML_Binary_Type type,const void *data,char *dst,int dst_size)
{
    switch (type)
    {
        case XBT_NULL: 
            return nullptr; // empty value
        case XBT_STRING: 
            return reinterpret_cast<const char *>(data); 
        case XBT_INT32: 
            snprintf(dst,dst_size,"%d",*reinterpret_cast<const int32_t*>(data));
            return dst;
        case XBT_UINT32: 
            snprintf(dst,dst_size,"%u",*reinterpret_cast<const uint32_t*>(data));
            return dst;
        case XBT_INT64:
            {
                int64_t value = *reinterpret_cast<const int64_t*>(data);   
                if (value == 0) return "0";
                if (value == -576460752303423488) return "9223372036854775808";
                //if (value == -0x8000000000000000) return "9223372036854775808";
                char *d = dst+dst_size;
                bool neg = (value < 0);
                if (neg) value = -value;
                while (value > 0)
                {
                    *(--d) = '0'+(value % 10);
                    value /= 10;
                }
                if (neg) *(--d) = '-';
                return d;
            }
        case XBT_UINT64:
            {
                uint64_t value = *reinterpret_cast<const uint64_t*>(data);   
                if (value == 0) return "0";
                char *d = dst+dst_size;
                while (value > 0)
                {
                    *(--d) = '0'+(value % 10);
                    value /= 10;
                }
                return d;
            }
        case XBT_FLOAT:
            snprintf(dst,dst_size,"%f",*reinterpret_cast<const float*>(data));
            return dst;
        case XBT_UNIX_TIME:
            {
                uint32_t value = *reinterpret_cast<const uint32_t*>(data);
                time_t tm0 = (time_t)value;
                struct tm tm1;
                memset(dst,0,dst_size);

                gmtime_r(&tm0,&tm1);
                snprintf(dst,dst_size,"%04d-%02d-%02d %2d:%02d:%02d",1900+tm1.tm_year,1+tm1.tm_mon,tm1.tm_mday,tm1.tm_hour,tm1.tm_min,tm1.tm_sec);
                return dst;
            }
        case XBT_UNIX_TIME64_MSEC:
            {
                int64_t value = *reinterpret_cast<const int64_t*>(data);
                time_t tm0 = (time_t)(value/1000);
                struct tm tm1;
                memset(dst,0,dst_size);

                gmtime_r(&tm0,&tm1);
                snprintf(dst,dst_size,"%04d-%02d-%02d %2d:%02d:%02d.%03d",1900+tm1.tm_year,1+tm1.tm_mon,tm1.tm_mday,tm1.tm_hour,tm1.tm_min,tm1.tm_sec,(int)(value % 1000));
                return dst;
            }
        case XBT_BLOB:
            {
                uint32_t size = *reinterpret_cast<const uint32_t*>(data);   
                const unsigned char *src = reinterpret_cast<const unsigned char*>(data)+4;   
                // char *base64_encode(const unsigned char *src,int src_size,char *dst,int dst_size)
                return base64_encode(src,size,dst,dst_size);
            }
        default:
            assert(false);
            return nullptr;
    }
    
    return dst;
}

}
