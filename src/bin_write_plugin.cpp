#ifdef BIN_WRITE_PLUGIN
#include <binary_xml/bin_write_plugin.h>

#include <binary_xml/bin_xml.h>
#include <binary_xml/bin_xml_types.h>
#include <binary_xml/macros.h>

#include <string.h>
#include <stdio.h>     // perror
#include <unistd.h>    // close,fstat
#include <stdarg.h>    // va_start ..
#include "assert.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <assert.h>

#define NOT_IMPLEMENTED false // used in assert
#define THIS reinterpret_cast<char*>(this)


// on Windows these flags are not supported
#ifndef O_NOATIME
    #define O_NOATIME 0
#endif
#ifndef MAP_NONBLOCK
    #define MAP_NONBLOCK 0
#endif

#define MAX_PARAM_KEYS 16   // good number of keys is 1, 3 is possible

namespace pklib_xml {

class BW_plugins
{
private:
    BW_plugin *plugins[MAX_BW_PLUGINS];
    int        plugins_count;
public:
    BW_plugins();
    virtual ~BW_plugins();

    bool registerPlugin(BW_plugin *plugin);
    bool unregisterPlugin(BW_plugin *plugin);
    BW_plugin *getPlugin(BW_pool *pool);
};

BW_plugins plugins;

//-------------------------------------------------------------------------------------------------
#undef WORK_ID
#define WORK_ID getPool()->getDocTypeName()

void BW_element::init(BW_pool *pool,int16_t identification,int8_t value_type,int8_t flags)
{
    if (this == nullptr)
    {
        LOG_ERROR("BW_element::init(NULL,id:%d=%s,value_type:%d=%s,flags:%u) called!",identification,
            (flags & BIN_WRITE_ELEMENT_FLAG ? pool->getTagName(identification) : pool->getAttrName(identification)),
            value_type,XBT2STR(value_type),flags);
        return;
    }
    assert(sizeof(*this) == 24);
    assert(pool != nullptr);

    int offset = THIS - reinterpret_cast<char*>(pool);
    assert(offset > 0);
    assert(offset+sizeof(BW_element) <= pool->file_size); //  pool->size); pool->size will be moved after commiting
    this->offset = offset;

    this->identification = identification;
    this->value_type = value_type;
    this->flags = flags;
    
//    printf("DBG: BW_element::init(ID=%d,T=%d,F=%d) --> (ID=%d,T=%d,F=%d)\n",
//        (int)identification,(int)value_type,(int)flags,
//        (int)this->identification,(int)this->value_type,(int)this->flags);

    this->next = offset;
    this->prev = offset;
    this->first_child = 0;
    this->first_attribute = 0;
}

BW_pool* BW_element::getPool() // const cannot be here
{
    return reinterpret_cast<BW_pool*>(THIS - offset);
}

BW_element* BW_element::BWE(BW_offset_t offset) //const cannot be here!
{
    return reinterpret_cast<BW_element*>(THIS - this->offset + offset);
}

char*       BW_element::BWD()
{
    return reinterpret_cast<char*>(this+1);
}

BW_element* BW_element::join(BW_element *B)    // this will connect two circles
// Input:
// A0-A1-A2-A3-A4-A5...An-A0
// B0-B1-B2-B3-B4-B5...Bn-B0
// Output:
// A0-A1-A2-A3-A4-A5...An-B0-B1-B2-B3-B4-B5...Bn-A0

{
    assert(this != nullptr);
    assert(B != nullptr);

    int A_prev_offset = this->prev;
    int B_prev_offset = B->prev;

    ASSERT_NO_RET_NULL(2092,A_prev_offset != 0);
    ASSERT_NO_RET_NULL(2093,B_prev_offset != 0);
    ASSERT_NO_RET_NULL(2094,this->offset != 0);
    ASSERT_NO_RET_NULL(2095,B->offset != 0);

    this->prev = B_prev_offset;
    B->prev = A_prev_offset;
    
    BWE(B_prev_offset)->next = this->offset;
    BWE(A_prev_offset)->next = B->offset;
    return this;
}

BW_element* BW_element::add(BW_element *tag)
{
    if (this == nullptr) return this; // no where to add
    if (tag == nullptr) return this; // adding no elements

    BW_offset_t *list = (tag->flags & BIN_WRITE_ELEMENT_FLAG) ? &first_child : &first_attribute;
    if (*list == 0)
        *list = tag->offset; // direct link to new element
    else
       BWE(*list)->join(tag); // join elements
    return this;
}

BW_element* BW_element::remove(BW_element *tag)
{
    if (this == nullptr) return nullptr;
    if (tag == nullptr) return this;

    BW_offset_t *list = (tag->flags & BIN_WRITE_ELEMENT_FLAG) ? &first_child : &first_attribute;
    if (*list == 0) return this; // empty list - cannot find
    if (*list == tag->offset)
    {
        if (tag->next == tag->offset && tag->prev == tag->offset)
            *list = 0; // removing last element -> empty
        else
            *list = tag->next;
    }
    BWE(tag->prev)->next = tag->next;
    BWE(tag->next)->prev = tag->prev;
    tag->next = tag->prev = tag->offset;
    tag->flags |= BIN_DELETED;
    return this;
}

BW_element* BW_element::replace(BW_element *old_value,BW_element *new_value)
{
    if (this == nullptr) return nullptr; // no where to add
    if (old_value == nullptr) return nullptr; // failure

    ASSERT_NO_RET_NULL(2016,new_value->next == new_value->offset);
    ASSERT_NO_RET_NULL(2017,new_value->prev == new_value->offset);
    ASSERT_NO_RET_NULL(2018,new_value->getPool() == old_value->getPool());

    new_value->next = old_value->next;
    new_value->prev = old_value->prev;

    BWE(new_value->next)->prev = new_value->offset;
    BWE(new_value->prev)->next = new_value->offset;
    
    old_value->flags |= BIN_DELETED;
    old_value->next = old_value->prev = old_value->offset;

    if (first_child == old_value->offset)
        first_child = new_value->offset;
    if (first_attribute == old_value->offset)
        first_attribute = new_value->offset;

    return this;
}



BW_element*     BW_element::attrNull(int16_t id)
{
    if (this == nullptr) return nullptr;
    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1985,attr_type == XBT_NULL || attr_type == XBT_VARIANT);
    
    if (attr_type == XBT_NULL)
    {
        BW_element* attr      = pool->new_element(XBT_NULL,0); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1986,attr != nullptr);
        attr->init(pool,id,XBT_NULL,BIN_WRITE_ATTR_FLAG);

        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1082,NOT_IMPLEMENTED); // TODO: variant
    }

    return this;
}

BW_element*     BW_element::attrStr(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
    if (value == nullptr) return this; // adding null value string is as nothing is added

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1050,attr_type == XBT_STRING || attr_type == XBT_VARIANT);
    
    int len = strlen(value);

    if (attr_type == XBT_STRING)
    {
        BW_element* attr      = pool->new_element(XBT_STRING,len); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1081,attr != nullptr);
        attr->init(pool,id,XBT_STRING,BIN_WRITE_ATTR_FLAG);

        char *dst             = reinterpret_cast<char*>(attr+1); // just after this element
        strcpy(dst,value);
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1987,NOT_IMPLEMENTED); // TODO: variant
    }

    return this;
}

BW_element*     BW_element::attrStr2(int16_t id,const char *beg,const char *end)
{
    if (this == nullptr) return nullptr;
    if (beg == nullptr) return this; // adding null value string is as nothing is added
    if (end == nullptr) return this; // adding null value string is as nothing is added

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1186,attr_type == XBT_STRING || attr_type == XBT_VARIANT);
    
    int len = end - beg;

    if (attr_type == XBT_STRING)
    {
        BW_element* attr      = pool->new_element(XBT_STRING,len); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1187,attr != nullptr);
        attr->init(pool,id,XBT_STRING,BIN_WRITE_ATTR_FLAG);

        char *dst             = reinterpret_cast<char*>(attr+1); // just after this element
        strncpy(dst,beg,len); 
        dst[len] = '\0';
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1188,NOT_IMPLEMENTED); // TODO: variant
    }

    return this;
}

BW_element*     BW_element::attrHexStr(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
    if (value == nullptr) return this; // adding null value string is as nothing is added
    ASSERT_NO_RET_NULL(1083,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrBLOB(int16_t id,const void *value,int32_t size)
{
    if (this == nullptr) return nullptr;
    
    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1084,attr_type == XBT_BLOB || attr_type == XBT_VARIANT);
    
    if (attr_type == XBT_STRING)
    {
        BW_element* attr      = pool->new_element(XBT_BLOB,size); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1085,attr != nullptr);

        attr->init(pool,id,XBT_BLOB,BIN_WRITE_ATTR_FLAG);

        char *dst             = reinterpret_cast<char*>(attr+1)+4; // just after this element and blob size
        memcpy(dst,value,size);
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1086,NOT_IMPLEMENTED); // TODO: variant
    }

    ASSERT_NO_RET_NULL(1087,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrInt32(int16_t id,int32_t value,BW_element **dst_attr)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1088,attr_type == XBT_INT32);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1089,attr != nullptr);
    attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

    int32_t *dst          = reinterpret_cast<int32_t*>(attr+1); // just after this element
    *dst                  = value; // store value

    add(attr);
    if (dst_attr != nullptr)
        *dst_attr = attr;

    return this;
}

BW_element*     BW_element::attrUInt32(int16_t id,uint32_t value)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1189,attr_type == XBT_UINT32);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1190,attr != nullptr);
    attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

//    LOG("UInt32: id=%d value=%u",id,value);

    
    uint32_t *dst         = reinterpret_cast<uint32_t*>(attr+1); // just after this element
    *dst                  = value; // store value

    add(attr);

    return this;
}

BW_element*     BW_element::attrInt64(int16_t id,int64_t value)
{
    if (this == nullptr) return nullptr;
    
    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1167,attr_type == XBT_INT64 || attr_type == XBT_VARIANT);

    if (attr_type == XBT_INT64)
    {
        BW_element* attr      = pool->new_element(XBT_INT64); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1168,attr != nullptr);
        attr->init(pool,id,XBT_INT64,BIN_WRITE_ATTR_FLAG);

        int64_t *dst          = reinterpret_cast<int64_t*>(attr+1); // just after this element
        *dst                  = value; // store value
        
        add(attr);
    }
    else
        ASSERT_NO_RET_NULL(1091,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element  *BW_element::attrUInt64(int16_t id,uint64_t value)
{
    if (this == nullptr) return nullptr;
    
    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1169,attr_type == XBT_UINT64 || attr_type == XBT_VARIANT);

    if (attr_type == XBT_UINT64)
    {
        BW_element* attr      = pool->new_element(XBT_UINT64); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1170,attr != nullptr);
        attr->init(pool,id,XBT_UINT64,BIN_WRITE_ATTR_FLAG);

        uint64_t *dst          = reinterpret_cast<uint64_t*>(attr+1); // just after this element
        *dst                  = value; // store value
        
        add(attr);
    }
    else
        ASSERT_NO_RET_NULL(1162,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrFloat(int16_t id,float value)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1961,attr_type == XBT_FLOAT);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1962,attr != nullptr);
    attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

    float *dst            = reinterpret_cast<float*>(attr+1); // just after this element
    *dst                  = value; // store value

    add(attr);

    return this;
}

BW_element*     BW_element::attrDouble(int16_t id,double value)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1963,attr_type == XBT_DOUBLE);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1964,attr != nullptr);
    attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

    double *dst            = reinterpret_cast<double*>(attr+1); // just after this element
    *dst                  = value; // store value

    add(attr);

    return this;
}

BW_element*     BW_element::attrGUID(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1094,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrSHA1(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1095,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrTime(int16_t id,time_t value)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1163,attr_type == XBT_UNIX_TIME || attr_type == XBT_VARIANT);

    if (attr_type == XBT_UNIX_TIME)
    {
        BW_element* attr      = pool->new_element(XBT_UNIX_TIME); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1164,attr != nullptr);
        attr->init(pool,id,XBT_UNIX_TIME,BIN_WRITE_ATTR_FLAG);

        uint32_t *dst         = reinterpret_cast<uint32_t*>(attr+1); // just after this element
        *dst                  = (uint32_t)value; // store value
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1096,NOT_IMPLEMENTED); // TODO: variant
    }

    return this;
}

BW_element* BW_element::attrTime64(int16_t id,int64_t value)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1931,attr_type == XBT_UNIX_TIME64_MSEC || attr_type == XBT_VARIANT);

    if (attr_type == XBT_UNIX_TIME64_MSEC)
    {
        BW_element* attr      = pool->new_element(XBT_UNIX_TIME64_MSEC); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1932,attr != nullptr);
        attr->init(pool,id,XBT_UNIX_TIME64_MSEC,BIN_WRITE_ATTR_FLAG);

        int64_t *dst         = reinterpret_cast<int64_t*>(attr+1); // just after this element
        *dst                 = value; // store value
//        LOG("Time64: id=%d value=%08x-%08x",id,(uint32_t)(value),(uint32_t)(value >> 32));
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1933,NOT_IMPLEMENTED); // TODO: variant
    }

    return this;
}

BW_element*     BW_element::attrIPv4(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1097,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrIPv6(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1098,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrData(int16_t id,XML_Binary_Data_Ref &data)
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);

    if (data.type == XBT_STRING && attr_type != XBT_STRING)
    { // default conversion
        // this is not so easy
        // I must know the size of target representation before I start
        char *buffer = reinterpret_cast<char*>(alloca(data.size+64)); // binary representation should fit into it's string representation size
        char *p = buffer;
        //bool XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit)
        ASSERT_NO_RET_NULL(1995,XBT_FromString(data.data,attr_type,&p,buffer+data.size+64));
        int target_data_size = p - buffer;

        BW_element* attr      = reinterpret_cast<BW_element*>(pool->allocate8(sizeof(BW_element)+target_data_size));
        ASSERT_NO_RET_NULL(1996,attr != nullptr);

        attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

        memcpy(attr+1,buffer,target_data_size);

        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1992,attr_type == data.type);

        BW_element* attr      = reinterpret_cast<BW_element*>(pool->allocate8(sizeof(BW_element)+data.size));
        ASSERT_NO_RET_NULL(1993,attr != nullptr);

        attr->init(pool,id,attr_type,BIN_WRITE_ATTR_FLAG);

        memcpy(attr+1,data.data,data.size);

        add(attr);
    }
    return this;

}

BW_element*     BW_element::attrCopy(XB_reader &xb,XML_Item *X,XML_Param_Description *param_desc)
{
    if (this == nullptr) return nullptr;
    XML_Binary_Data_Ref data = param_desc->getData(X);
    return attrData(param_desc->name,data);
}

BW_element  *BW_element::attrGet(int16_t id)
{
    if (this == nullptr) return nullptr;
    if (first_attribute == 0) return nullptr;

    BW_element *A = BWE(first_attribute);
    for(;;)
    {
        if (A->identification == id)
            return A;
        if (A->next == first_attribute) break; // not found
        A = BWE(A->next);
    }
    return nullptr;
}

BW_element*  BW_element::NextChild(BW_offset_t &offset)
// Usage:
// for(BW_offset_t idx = 0,BW_element *X;X != nullptr;X = NextChild(idx))
// 
{
    if (this == nullptr) return nullptr; // empty list -> no children

    if (offset == 0) // first element of loop
    {
        if (first_child == 0) 
            return nullptr; // empty list
        else
            return BWE(offset = first_child); // first child
    }   

    BW_element *prev = BWE(offset); // current element, need next
    if (prev->next == first_child) return nullptr; // end of loop
    return BWE(offset = prev->next);
}

//-------------------------------------------------------------------------------------------------

BW_element  *BW_element::tagGet(int16_t id)
{
    if (this == nullptr) return nullptr;
    if (first_child == 0) return nullptr;

    BW_element *A = BWE(first_child);
    for(;;)
    {
        if (A->identification == id)
            return A;
        if (A->next == first_child) break; // not found
        A = BWE(A->next);
    }
    return nullptr;
}

BW_element  *BW_element::tag(int16_t id) // find or create
{
    if (this == nullptr) return nullptr;
    BW_element *target = tagGet(id);
    if (target != nullptr) return target;
    target = getPool()->tag(id);
    add(target);

    return target;
}

BW_element  *BW_element::tagSetInt32(int16_t id,int32_t value)
{
    if (this == nullptr) return nullptr;

    BW_element *target = tagGet(id);
    if (target == nullptr) // no previous value - ok, create new
    {
        target = getPool()->tagInt32(id,value);
        add(target);
    }
    else
    {
        bool to_replace = (target->value_type != XBT_INT32);
        if (!to_replace) // check size
        {
            int32_t *dst = reinterpret_cast<int32_t*>(target+1);
            *dst = value;
        }
        else
        {
            BW_element *new_item = getPool()->tagInt32(id,value);
            replace(target,new_item);
        }
         
    }
    return target;
}

BW_element  *BW_element::tagSetInt64(int16_t id,int64_t value)
{
    if (this == nullptr) return nullptr;

    BW_element *target = tagGet(id);
    if (target == nullptr) // no previous value - ok, create new
    {
        target = getPool()->tagInt64(id,value);
        add(target);
    }
    else
    {
        bool to_replace = (target->value_type != XBT_INT64);
        if (!to_replace) // check size
        {
            int64_t *dst = reinterpret_cast<int64_t*>(target+1);
            *dst = value;
        }
        else
        {
            BW_element *new_item = getPool()->tagInt64(id,value);
            replace(target,new_item);
        }
         
    }
    return target;
}

BW_element  *BW_element::tagSetString(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;

    BW_element *target = tagGet(id);
    if (target == nullptr) // no previous value - ok, create new
    {
        target = getPool()->tagString(id,value);
        add(target);
    }
    else
    {
        bool to_replace = (target->value_type != XBT_STRING);
        if (!to_replace) // check size
        {
            int value_len = (value != nullptr ? strlen(value) : 0);
            char *dst = reinterpret_cast<char*>(target+1);
            int target_size = strlen(dst);
            if (((value_len+1+3) >> 2) <= (target_size+1+3) >> 2)
                strcpy(dst,value); // shorter or in the same allocation size
            else
                to_replace = true;
        }
        if (to_replace)
        {
            BW_element *new_item = getPool()->tagString(id,value);
            replace(target,new_item);
        }
         
    }

    return target;
}

BW_element  *BW_element::tagSetTime(int16_t id,time_t value)
{
    if (this == nullptr) return nullptr;
    BW_element *target = tagGet(id);
    if (target != nullptr)
    {
        time_t *tm = reinterpret_cast<time_t*>(target+1);
        *tm = value;
    }
    else
    {
        target = getPool()->tagTime(id,value);
        add(target);
    }

    return target;
}

BW_element  *BW_element::tagSetSHA1(int16_t id,const uint8_t *value)
{
    if (this == nullptr) return nullptr;
    BW_element *target = tagGet(id);
    if (target != nullptr)
    {
        ASSERT_NO_RET_NULL(2019,target->value_type == XBT_SHA1);
        if (value == nullptr)
            memset(target+1,0,20);
        else
            memcpy(target+1,value,20);
    }
    else
    {
        target = getPool()->tagSHA1(id,value);
        add(target);
    }

    return target;
}

//-------------------------------------------------------------------------------------------------

int32_t *BW_element::getInt32()
{
    if (this == nullptr) return nullptr;

//    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = getSymbolType();
    if (attr_type != XBT_INT32)
    {
        LOG_ERROR("[1173] +%d %s'type = %d = %s but XBT_INT32 is expected!",offset,getName(),attr_type,XBT2STR(attr_type));
        return nullptr;
    }

    return reinterpret_cast<int32_t*>(this+1); // just after this element
}

int64_t *BW_element::getInt64()
{
    if (this == nullptr) return nullptr;

//    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = getSymbolType();
    if (attr_type != XBT_INT64 && value_type != XBT_INT64 && value_type != XBT_UNIX_TIME64_MSEC)
    {
        //ASSERT_NO_RET_NULL(2026,attr_type == XBT_INT64 || value_type == XBT_INT64);// || attr_type == XBT_VARIANT);
        LOG_ERROR("[2026] +%d %s'type = %d/%d = %s/%s but XBT_INT64/XBT_UNIX_TIME64_MSEC is expected!",
        offset,getName(),
        attr_type,value_type,XBT2STR(attr_type),XBT2STR(value_type));
        return nullptr;
    }


    return reinterpret_cast<int64_t*>(this+1); // just after this element
}

uint64_t *BW_element::getUInt64()
{
    if (this == nullptr) return nullptr;

    XML_Binary_Type     attr_type = getSymbolType();
    if (attr_type != XBT_UINT64)
    {
        LOG_ERROR("[2096] +%d %s'type = %d/%d = %s/%s but XBT_UINT64 is expected!",offset,getName(),
        attr_type,value_type,XBT2STR(attr_type),XBT2STR(value_type));
        return nullptr;
    }

    return reinterpret_cast<uint64_t*>(this+1); // just after this element
}


char *BW_element::getStr()
{
    if (this == nullptr) return nullptr;
    if (value_type != XBT_STRING) return nullptr;
    return reinterpret_cast<char*>(this+1);
}

XML_Binary_Data_Ref BW_element::getData() 
{
    XML_Binary_Data_Ref R = {type:XBT_UNKNOWN,data:nullptr,size:0};
    if (this == nullptr) return R;
    R.data = reinterpret_cast<char*>(this+1);
    R.type = static_cast<XML_Binary_Type>(value_type);
    if (value_type == XBT_STRING) 
        R.size = 1+strlen(R.data);
    else if (XBT_IS_4(value_type)) 
        R.size = 4;
    else if (XBT_IS_VARSIZE(value_type)) 
        R.size = 4 + *reinterpret_cast<int32_t*>(R.data);
    else 
        R.size = XBT_FIXEDSIZE(value_type);
    return R;
}

BW_element  *BW_element::findChildByTag(int16_t tag_id)
// the same like tagGet(id)
{
    if (this == nullptr) return nullptr;
    // for all children
    
    if (first_child == 0) return nullptr; // no children

    BW_element *child = BWE(first_child);
    for(;;)
    {
        if (child->identification == tag_id) // child has requested tag_id
            return child;

        if (child->next == first_child) break; // finished
        child = BWE(child->next);
    }
    return nullptr;
}

BW_element  *BW_element::findChildByParam(int16_t tag_id,int16_t attr_id,XML_Binary_Type value_type,void *data,int data_size)
{
    if (this == nullptr) return nullptr;
    // for all children
    
    if (first_child == 0) return nullptr; // no children

    BW_element *child = BWE(first_child);
    for(;;)
    {
        if (child->identification == tag_id) // child has requested tag_id
        {
            if (child->first_attribute != 0) // has attributes!
            {
                BW_element *attr = BWE(child->first_attribute);
                for(;;)
                {
                    if (attr->identification == attr_id) // ok, now I will know!
                    {
                    // I have value_type, data & data_size to compare with ...
                        int cmp = XBT_Compare(static_cast<XML_Binary_Type>(attr->value_type),reinterpret_cast<void*>(attr+1),
                                                XBT_Size2(static_cast<XML_Binary_Type>(attr->value_type),0),
                                              value_type,data,data_size);
                        if (cmp == 0)
                            return child;
                    }
                    
                    if (attr->next == child->first_attribute) break; // finished
                    attr = BWE(attr->next);
                }
            }
        }

        if (child->next == first_child) break; // finished
        child = BWE(child->next);
    }
    return nullptr;
}

BW_element  *BW_element::findAttr(int16_t attr_id)
{
    if (this == nullptr) return nullptr;
    // for all children
    
    if (first_attribute == 0) return nullptr; // no children

    BW_element *attr = BWE(first_attribute);
    for(;;)
    {
        if (attr->identification == attr_id) // attr has requested attattrd
            return attr;

        if (attr->next == first_attribute) break; // finished
        attr = BWE(attr->next);
    }
    return nullptr;
}

BW_element*  BW_element::CopyKeys(XB_reader &xb,XML_Item *src)
{
// must be empty element
    ASSERT_NO_RET_NULL(1978,first_child == 0);
    ASSERT_NO_RET_NULL(1979,first_attribute == 0);
    ASSERT_NO_RET_NULL(1981,identification == (int16_t)src->name); // must be the same element type

    for(int i = 0;i < src->paramcount;i++)
    {
        XML_Param_Description *PD = src->getParamByIndex(i);
        if (!xb.ParamIsKey(PD->name)) continue; // skip this param - it is not 
        ASSERT_NO_RET_NULL(2000,this->attrCopy(xb,src,PD) == this);
    }
    return this;
}

bool         BW_element::EqualKeys(XB_reader &xb,XML_Item *src)
// This should compare all key values from src and this
// when some element miss, it should return false
{
    if (src == nullptr) return false;

    // I must remember all processed keys
    int checked_params[MAX_PARAM_KEYS];
    int checked_params_count = 0;

    // O(n*n)
    for(int p = 0;p < src->paramcount;p++)
    {
        XML_Param_Description *pd = src->getParamByIndex(p);
        if (!xb.ParamIsKey(pd->name)) continue;
        
        // for all params
        if (first_attribute == 0) return false; // no key
        BW_element *child = BWE(first_attribute);
        for(;;)
        {
            if (child->identification == pd->name)
            {
                XML_Binary_Data_Ref pd_data = pd->getData(src); 
                if (pd_data.data == nullptr) return false;

                XML_Binary_Data_Ref bw_data = child->getData(); 
                if (bw_data.data == nullptr) return false;
    
                if (!XBT_Equal(pd_data,bw_data)) return false;            


                // OK, equal
                break; // found and equal
            }
            if (child->next == first_attribute) return false; // not found key
            child = BWE(child->next);
        }

        ASSERT_NO_RET_FALSE(1990,checked_params_count < MAX_PARAM_KEYS);
        checked_params[checked_params_count++] = pd->name;
    }
// BW_element pass
    if (first_attribute != 0)
    {
        BW_element *child = BWE(first_attribute);
        for(;;)
        {
            if (xb.ParamIsKey(child->identification))
            {
                bool ok = false;
                for(int i = 0;!ok && i < checked_params_count;i++)
                    ok = (child->identification == checked_params[i]);
                if (!ok) return false; // not found
            }
            if (child->next == first_attribute) break;
            child = BWE(child->next);
        }
    }
// OK - equal
    return true;
}

//-------------------------------------------------------------------------------------------------
#undef WORK_ID
#define WORK_ID pool->getDocTypeName()

int16_t BW_symbol_table_16B::getByName(BW_pool *pool,const char *name,BW_offset_t *offset,XML_Binary_Type *type)
{
    ASSERT_NO_RET_N1(1969,pool != nullptr);
    char *POOL = reinterpret_cast<char*>(pool);

    if (index == 0) // sequential search
    {
        int id = 0;
// ID:word|Type:byte|string|NUL:byte
        const char *p =       POOL+names_offset;
        const char *p_limit = POOL+pool->allocator;

        while (id <= max_id)
        { 
            const char *n = name;
            const char *start = p;
            id = *(p++);
            id |= *(p++) << 8;
    
            XML_Binary_Type element_type = static_cast<XML_Binary_Type>(*(p++));

            // compare symbol and name
            for(;p < p_limit && *n == *p && *n != '\0';p++,n++);
            // result?
            if (p >= p_limit) return -1; // not found
            if (*p == '\0' && *n == '\0') 
            {
                if (offset != nullptr)
                    *offset = start-POOL;
                if (type != nullptr)
                    *type = element_type;
                return id; // found
            }

            // skip rest of identifier
            while(p < p_limit && *p != '\0') p++; // find end of string
            if (p >= p_limit) return -1; // not found

            AA(p); // round 4B
            // next id
            id++;
        }
    }
    else
    {
    // OK, i have indexed access and this should be faster
        BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(POOL+index);
        for(int id = 0;id <= max_id;id++)
        {
            const char *start = POOL+elements[id]; 
            if (strcmp(name,start+3) != 0) continue; // NEXT!
        //------FOUND---------
            if (offset != nullptr)
                *offset = elements[id];
            if (type != nullptr)
                *type = static_cast<XML_Binary_Type>(start[2]);
            return id;
        }
    }
    return -1; // not found
}

const char *BW_symbol_table_16B::getName(BW_pool *pool,int search_id)
{
    ASSERT_NO_RET_NULL(2032,pool != nullptr);
    ASSERT_NO_RET_NULL(2033,search_id >= 0 && search_id <= max_id); // TODO: some negative ID are supported

    char *POOL = reinterpret_cast<char*>(pool);

    if (index == 0) // sequential search
    {
        int id = 0;
        // ID:word|Type:byte|string|NUL:byte
        const char *p =       POOL+names_offset;
        const char *p_limit = POOL+pool->allocator;

        while (id <= max_id)
        { 
            //const char *start = p;
            id = *(p++);
            id |= *(p++) << 8;

            //XML_Binary_Type element_type = static_cast<XML_Binary_Type>(*(p++));

            if (id == search_id) return p; // found

            // compare symbol and name
            while(p < p_limit && *p != '\0') p++;
            // result?
            if (p >= p_limit) return nullptr; // not found
            p++; // skip ASCII.NUL

            AA(p); // round 4B
            // next id
            id++;
        }
        return nullptr;
    }
    // OK, i have indexed access and this should be faster
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(POOL+index);
    const char *start = POOL+elements[search_id]; 
    return start+3;
}

int32_t BW_symbol_table_16B::getNamesSize(BW_pool *pool)
// sometimes I need size of whole sequence of names
// for copy when extending old table
{
    ASSERT_NO_RET_N1(2097,pool != nullptr);
    if (names_offset == 0) return 0;
    
    char *POOL = reinterpret_cast<char*>(pool);
    const char *start_p = POOL+names_offset;
    const char *p_limit = POOL+pool->allocator;
    const char *p = start_p;
    
    int id = 0;
    int prev_id = -1;
    while (id <= max_id)
    {
            id = *(p++);
            id |= *(p++) << 8;
            if (id != prev_id+1) // don't go to other table
            {
                LOG_ERROR("%s/%s correcting max_id: %d --> %d",pool->getDocTypeName(),(&pool->tags == this ? "tags" : "params"),max_id,prev_id);
                max_id = prev_id;
                p -= 2;
                break;
            }
            p++; // skip type
            while(p < p_limit && *p != '\0') p++;
            p++; // ASCII.NUL
            AA(p);
            prev_id = id;
            id++;
    }
    return p - start_p;
}

bool BW_symbol_table_16B::check(BW_pool *pool)
{
    if (index == 0) return true; // OK, no index is OK
    char *POOL = reinterpret_cast<char*>(pool);
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(POOL+index);
    int names_size = getNamesSize(pool);
    for(int i = 0;i <= max_id;i++)
    {
        // index reference must be in range of correct names, otherwise index is invalid
        ASSERT_NO_RET_FALSE(2109,elements[i] >= names_offset && elements[i] < names_offset + names_size);
    }
    return true; // OK
}

bool BW_symbol_table_16B::Open(BW_pool *pool)
{
    ASSERT_NO_RET_FALSE(2100,pool != nullptr);
    int size = getNamesSize(pool);
    if (index == 0 && names_offset + size == pool->allocator) return true; // no index means - it is opened
// prepare new copy of symbol tables with open end
    LOG("%s(%s/%s) - opening symbol table (max_id=%d,size=%d,offset.old=%d,allocator=%d (should be %d)",
                __FUNCTION__,pool->getDocTypeName(),(&pool->tags == this ? "tags" : "params"),max_id,
                size,names_offset,pool->allocator,names_offset+size);
    char *names = pool->allocate(size);
    ASSERT_NO_RET_FALSE(2101,names != nullptr);
    memcpy(names,(char*)pool + names_offset,size);
    index = 0; // no index yet
    names_offset = names - (char *)pool;
    return true;
}

//-------------------------------------------------------------------------------------------------
#undef WORK_ID
#define WORK_ID getDocTypeName()

XML_Binary_Type BW_pool::getTagType(int16_t id) 
{
    ASSERT_NO_(1063,this != nullptr,return XBT_UNKNOWN);
    ASSERT_NO_(1051,tags.index != 0,return XBT_UNKNOWN);                // index not initialized 

    if (id >=0 && id <= tags.max_id)    // id in range
    {
        BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+tags.index);
        BW_offset_t offset = elements[id];
        ASSERT_NO_(1053,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
        return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(THIS + offset + 2));  
    }

    if (id < 0 && id > XTNR_LAST) // some default tags
        switch (id)
        {
            case XTNR_NULL: return XBT_UNKNOWN;                     // undefined
            case XTNR_META_ROOT: return XBT_NULL;                   // meta_root    
            case XTNR_TAG_SYMBOLS: return XBT_BLOB;               // tag_symbols
            case XTNR_PARAM_SYMBOLS: return XBT_BLOB;             // param_symbols
            case XTNR_HASH_INDEX: return XBT_UNKNOWN;               // hash_index -- not implemented yet
            case XTNR_PAYLOAD: return XBT_NULL;                     // payload = content of XML file
            case XTNR_REFERENCE: return XBT_UNKNOWN;                // this is reference tag!! - it is exact copy of other tag
            case XTNR_PROCESSED: return XBT_UNKNOWN;                // this tag was processed and in .reference field is it's new address
            case XTNR_ET_TECERA: return XBT_NULL;                   // this tag is last tag in file and is placeholder for other tags appended to the file later without modification of core
            default:
                break;
        }

    std::cerr << ANSI_RED_BRIGHT "[1052] " << __FUNCTION__ << ":" ANSI_RED_DARK " Unknown tag type " << id << "!" ANSI_RESET_LF;
    return XBT_UNKNOWN;    // id out of range - should be in range, because range is defined by writing application
}

const char*     BW_pool::getTagName(int16_t id)
{
    ASSERT_NO_RET_NULL(1064,this != nullptr);
    ASSERT_NO_RET_NULL(1054,tags.index != 0);                // index not initialized 
    if (id < 0) return XTNR2str(id);
    ASSERT_NO_RET_NULL(1055,id >=0 && id <= tags.max_id);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+tags.index);
    
    BW_offset_t offset = elements[id];
    ASSERT_NO_RET_NULL(1056,offset != 0);                                       // tag id should be correctly registered!
    return reinterpret_cast<const char *>(THIS+ offset + 3);  
}


XML_Binary_Type BW_pool::getAttrType(int16_t id)
{
    ASSERT_NO_(1065,this != nullptr,return XBT_UNKNOWN);
    ASSERT_NO_(1057,params.index != 0,return XBT_UNKNOWN);                // index not initialized 
    if (id < 0)
    {
        switch (id)
        {
            case XPNR_NULL : return XBT_NULL;
            case XPNR_NAME : return XBT_STRING;
            case XPNR_HASH : return XBT_SHA1;
            case XPNR_MODIFIED : return XBT_UNIX_TIME;
            case XPNR_COUNT : return XBT_INT32;
            case XPNR_FORMAT_VERSION : return XBT_INT32;
            case XPNR_TYPE_COUNT : return XBT_INT32;
            case XPNR_ID         : return XBT_INT32;
            case XPNR_AUTHOR     : return XBT_INT64;
            case XPNR_TIME_MS    : return XBT_UNIX_TIME64_MSEC;
            default: break;
        }
    }
    ASSERT_NO_(1058,id >=0 && id <= params.max_id,return XBT_UNKNOWN);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+params.index);
    BW_offset_t offset = elements[id];

    if (offset == 0) return XBT_UNKNOWN;    // empty id
    //ASSERT_NO_(1059,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(THIS+ offset + 2));  
}

const char*     BW_pool::getAttrName(int16_t id)
{
    ASSERT_NO_RET_NULL(1066,this != nullptr);
    ASSERT_NO_RET_NULL(1060,params.index != 0);                // index not initialized 

    switch (id)
    {
        case XPNR_NULL : return "NULL";
        case XPNR_NAME : return "name";
        case XPNR_HASH : return "hash";
        case XPNR_MODIFIED : return "modified";
        case XPNR_COUNT : return "count";
        case XPNR_FORMAT_VERSION : return "version";
        case XPNR_TYPE_COUNT : return "type_count";
        case XPNR_ID : return "id";
        case XPNR_AUTHOR : return "author";
        case XPNR_TIME_MS : return "time_ms";

        default:
        {
           ASSERT_NO_RET_NULL(1061,id >=0 && id <= params.max_id);    // id out of range - should be in range, because range is defined by writing application
           BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+params.index);
           BW_offset_t offset = elements[id];
           ASSERT_NO_RET_NULL(1062,offset != 0);                                       // tag id should be correctly registered!
           return reinterpret_cast<const char *>(THIS+ offset + 3);  
        }
    }
}

bool   BW_pool::makeTable(BW_symbol_table_16B &table)
// This is called after registration of symbols to table - see registerTag & registerAttr
{
    table.index = 0;
    if (table.max_id < 0) return true; // ok, empty array

    ASSERT_NO_RET_FALSE(1111,table.names_offset != 0);

    
    int32_t *index = reinterpret_cast<int32_t*>(allocate(sizeof(int32_t) * (table.max_id+1)));
    ASSERT_NO_RET_FALSE(1118,index != nullptr); // out of memory?

    BW_offset_t start = table.names_offset;
    BW_offset_t end = table.names_offset+table.getNamesSize(this);
    
    for(int i = 0;i <= table.max_id;i++)
    {
        ASSERT_NO_RET_FALSE(2112,start < end);
        int id = *reinterpret_cast<int16_t*>(THIS + start);
        if (id < 0 || id > table.max_id)
        {
            LOG_ERROR("[A1112] %s - found id %d at index %d/%d - name %s",getDocTypeName(),id,i,table.max_id+1,THIS+start+3);
            return false;
        }
        //ASSERT_NO_RET_FALSE(2113,id == i);
        //ASSERT_NO_RET_FALSE(1112,id >= 0 && id <= table.max_id);
        index[id] = start;
        start += 3 + strlen(THIS+start+3)+1;
        ROUND32UP(start);
    }
    //ASSERT_NO_RET_FALSE(1113,start == end);
    table.index = reinterpret_cast<char*>(index) - THIS;
    return true;
}

bool   BW_pool::checkTable(BW_symbol_table_16B &table,BW_offset_t limit)
{
// I must check, the symbol table is ok and valid!    
    if (table.max_id < 0) return true; // no symbols = OK
    ASSERT_NO_RET_FALSE(1938,table.index >= (int)sizeof(*this));
    int32_t *index = reinterpret_cast<int32_t*>(THIS + table.index);
    for(int i = 0;i <= table.max_id;i++)
    {
        if (index[i] == 0) continue; // empty - ok
        ASSERT_NO_RET_FALSE(1940,index[i] >= table.names_offset);
        ASSERT_NO_RET_FALSE(1941,index[i] < table.names_offset);
        
    }
    return true;
}

bool   BW_pool::check_id()
{
    return strcmp(binary_xml_write_type_info,"binary_xml.pksoft.org") == 0;
}

const char *BW_pool::getDocTypeName() 
// For debugging purposes - which XBW has problem?
{
    if (this == nullptr) return "(NULL)";
//    if (pool_format_version >= BIN_WRITE_POOL_FORMAT_VERSION_WITH_PARENT)
//    {
//        return doc_type_name;
//    }
    if (root > getPoolSize() && root < allocator)
    {
        BW_element *root = BWE(this->root);   
        ASSERT_NO_RET_(2114,root->flags & BIN_WRITE_ELEMENT_FLAG,"(bad root flags)");
        const char *name = tags.getName(this,root->identification);    
        if (name != nullptr) return name;
    }
// OK, no root name - so some early stage
   
    BW_plugin *plugin = plugins.getPlugin(this);
    if (plugin == nullptr) return "(no plugin for pool)";
    const char *filename = plugin->getFilename();
    if (filename == nullptr) return "(no filename in plugin)";
    const char *slash = strrchr(filename,'/');
    if (slash != nullptr) return slash+1;
    return filename;
}

char*  BW_pool::allocate(int size)
{
    if (size <= 0) return 0;
    ROUND32UP(size);
// memory must be prepared in allocator_limit before allocation - in BW_pool is fd inaccessible
    ASSERT_NO_RET_0(1080,allocator + size <= allocator_limit);

    BW_offset_t offset = allocator;
    allocator += size;

    //MAXIMIZE(size,allocator);BUG!!
    memset(THIS+offset,0,size);

    return THIS+offset;
}

char*           BW_pool::allocate8(int size)        // 64 bit aligned value
{
    if (size <= 0) return 0;
    ROUND64UP(allocator);
    ROUND64UP(size);
// memory must be prepared in allocator_limit before allocation - in BW_pool is fd inaccessible
    ASSERT_NO_RET_0(1942,allocator + size <= allocator_limit);

    BW_offset_t offset = allocator;
    allocator += size;

    //MAXIMIZE(size,allocator);BUG!!
    memset(THIS+offset,0,size);

    return THIS+offset;
}

BW_element*     BW_pool::new_element(XML_Binary_Type type,int size)
// size is content only size
// XBT_Size() will add envelope size
{
    ASSERT_NO_RET_NULL(1067,this != nullptr);
    int size2 = XBT_Size2(type,size);
    ASSERT_NO_RET_NULL(2075,size2 >= 0);
    //if (size2 < 0) return nullptr;
    int align = XBT_Align(type);
    ASSERT_NO_RET_NULL(2068,align == 0 || align == 4 || align == 8);
    BW_element* result = reinterpret_cast<BW_element*>(align == 8 ? allocate8(sizeof(BW_element)+size2) : allocate(sizeof(BW_element)+size2));
    if (result == nullptr) // need makeSpace
    {
        BW_plugin *plugin = plugins.getPlugin(this);
        ASSERT_NO_RET_NULL(2012,plugin != nullptr);
        ASSERT_NO_RET_NULL(2013,plugin->makeSpace(sizeof(BW_element)+size2));
        result = reinterpret_cast<BW_element*>(align == 8 ? allocate8(sizeof(BW_element)+size2) : allocate(sizeof(BW_element)+size2));
        ASSERT_NO_RET_NULL(2091,result != nullptr);
        if (result == nullptr) return nullptr; // error message already shown in allocate
    }
    result->value_type = type;
    result->prev = result->next = result->offset = (reinterpret_cast<char*>(result) - reinterpret_cast<char*>(this));
    switch(type)
    {
        case XBT_BLOB:
        case XBT_HEX:
        // size is stored to the 1st byte of data
            *reinterpret_cast<int32_t*>(1+result) = size2 - sizeof(int32_t);
            break;
        default:
            break;
    }
    return result;
}

#define CHECK_TAG_TYPE(no,id,type)\
    XML_Binary_Type tag_type = getTagType(id);\
    if (tag_type != type)\
    {\
        LOG_ERROR("[%d] Tag %s has type %d=%s and not %s!",no,getTagName(id),tag_type,XBT2STR(tag_type),#type);\
        return nullptr;\
    }
BW_element* BW_pool::tag(int16_t id)
{
    CHECK_TAG_TYPE(2014,id,XBT_NULL);
    BW_element* result = new_element(XBT_NULL,0);
    ASSERT_NO_RET_NULL(2034,result != nullptr);
    
    result->init(this,id,XBT_NULL,BIN_WRITE_ELEMENT_FLAG);
    return result;
}

BW_element* BW_pool::tagInt32(int16_t id,int32_t value)
{
    CHECK_TAG_TYPE(2027,id,XBT_INT32);

    BW_element* result = new_element(XBT_INT32,0);
    ASSERT_NO_RET_NULL(2035,result != nullptr);
    
    result->init(this,id,XBT_INT32,BIN_WRITE_ELEMENT_FLAG);

    *reinterpret_cast<int32_t*>(result+1) = value;
    return result;
}

BW_element* BW_pool::tagInt64(int16_t id,int64_t value)
{
    CHECK_TAG_TYPE(2029,id,XBT_INT64);

    BW_element* result = new_element(XBT_INT64,sizeof(value));
    ASSERT_NO_RET_NULL(2036,result != nullptr);
    
    result->init(this,id,XBT_INT64,BIN_WRITE_ELEMENT_FLAG);

    *reinterpret_cast<int64_t*>(result+1) = value;
    return result;
}

BW_element* BW_pool::tagString(int16_t id,const char *value)
{
    CHECK_TAG_TYPE(2015,id,XBT_STRING);

    int value_len = (value != nullptr ? strlen(value) : 0);

    BW_element* result = new_element(XBT_STRING,value_len+1);
    ASSERT_NO_RET_NULL(2037,result != nullptr);
    
    result->init(this,id,XBT_STRING,BIN_WRITE_ELEMENT_FLAG);
    strcpy(reinterpret_cast<char*>(result+1),value);
    return result;
}

BW_element* BW_pool::tagTime(int16_t id,time_t value)
{
    CHECK_TAG_TYPE(2006,id,XBT_UNIX_TIME);

    BW_element* result = new_element(XBT_UNIX_TIME,0);
    ASSERT_NO_RET_NULL(2038,result != nullptr);
    
    result->init(this,id,XBT_UNIX_TIME,BIN_WRITE_ELEMENT_FLAG);
    
    *reinterpret_cast<uint32_t*>(result+1) = (uint32_t)value;
    return result;
}

BW_element* BW_pool::tagSHA1(int16_t id,const uint8_t *value)
{
    CHECK_TAG_TYPE(2028,id,XBT_SHA1);

    BW_element* result = new_element(XBT_SHA1,0);
    ASSERT_NO_RET_NULL(2039,result != nullptr);
    
    result->init(this,id,XBT_SHA1,BIN_WRITE_ELEMENT_FLAG);
    
    memcpy(result+1,value,20);
    return result;
}

BW_element* BW_pool::tagAny(int16_t id,XML_Binary_Type type,const char *data_as_string,int data_size)
{
    BW_element* result = new_element(type,data_size);
    ASSERT_NO_RET_NULL(2060,result != nullptr);
    result->init(this,id,type,BIN_WRITE_ELEMENT_FLAG);

    //bool XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit)
    char *wp = reinterpret_cast<char*>(result+1);
    ASSERT_NO_RET_NULL(2061,XBT_FromString(data_as_string,type,&wp,wp+data_size));

    return result;
}

//-------------------------------------------------------------------------------------------------
#undef WORK_ID
#define WORK_ID pool->getDocTypeName()

BW_plugin::BW_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size,uint32_t config_flags)
    : Bin_src_plugin(nullptr,bin_xml_creator)
// automatic change of extension
{
    this->max_pool_size = max_pool_size;
    this->pool = nullptr;
    this->fd = -1;
    this->initialized = false;
    this->check_only = false;
    this->check_failures = 0;
    this->config_flags = config_flags;
    

    Bin_src_plugin::setFilename(filename,".xbw"); // will allocate copy of filename
    ASSERT_NO_DO_NOTHING(2011,plugins.registerPlugin(this));
}

BW_plugin::~BW_plugin()
{
    ASSERT_NO_DO_NOTHING(2008,plugins.unregisterPlugin(this));
    ASSERT_NO_DO_NOTHING(1965,Finalize());
}

void BW_plugin::setFlags(uint32_t flags)
{
    this->config_flags = flags;
}

bool BW_plugin::setMaxPoolSize(int max_pool_size)
{
    ASSERT_NO_RET_FALSE(2048,!initialized);
    this->max_pool_size = max_pool_size;
    return true;
}

bool BW_plugin::Initialize()
{
    if (initialized) return true; // no double initialize!!
    ASSERT_NO_RET_FALSE(1952,filename != nullptr);

// BW_plugin is owner and the only one (for now) writer of mapped file

// int open(const char *pathname, int flags, mode_t mode);
    this->fd = open(filename,O_RDWR | O_CREAT | O_NOATIME ,S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd < 0)
    {
        ERRNO_SHOW(1048,"open",filename);
        return false;
    }

// I will ask for size of file
    struct stat statbuf;
    if (fstat(fd,&statbuf) < 0)
    {
        ERRNO_SHOW(1099,"fstat",filename);
        return false;
    }
    int file_size = (int)statbuf.st_size;
    ASSERT_NO_RET_FALSE(1100,file_size >= 0); // file should exist now

    for(int pass = 0;pass < 2;pass++)
    {
        //  void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
        this->pool = reinterpret_cast<BW_pool*>(mmap(nullptr,max_pool_size,PROT_READ | PROT_WRITE, 
                    MAP_SHARED |  //  Share  this  mapping.   Updates to the mapping are visible to other processes mapping the same region, 
                    // and (in the case of file-backed mappings) are carried through to the underlying file.  
                    // (To precisely control when updates are carried through to the underlying file requires the use of msync(2).)

                    MAP_NONBLOCK, // (since Linux 2.5.46) This flag is meaningful only in conjunction with MAP_POPULATE.  
                    // Don't perform read-ahead: create page tables entries only for pages that are already present in RAM.  
                    // Since Linux 2.6.23, this flag causes MAP_POPULATE to do nothing.  One day, the combination of MAP_POPULATE and MAP_NONBLOCK may be reimplemented.
                    fd, 0));
        if (pool == MAP_FAILED)
        {
            ERRNO_SHOW(1049,"mmap",filename);
            return false;
        }
        if (file_size < (int)sizeof(*pool)) file_size = 0; // too small - deleting
        ASSERT_NO_RET_FALSE(1948,file_size == 0 || (int)pool->mmap_size >= file_size); // must be allocated for whole size
        if (file_size == 0) break; // was empty -> OK
        if ((int)pool->mmap_size == max_pool_size) break; // correct size
        int new_pool_size = pool->mmap_size;
    
        if (munmap(this->pool,max_pool_size) != 0)
            ERRNO_SHOW(1949,"munmap",filename);
        this->pool = nullptr;

        max_pool_size = new_pool_size;

        ASSERT_NO_RET_FALSE(1950,pass < 1);
    }

// OK, what 2 do now?
// There are 2 situations - file is empty and I want to create and write ... simple one
//                        - file is somehow populated, it must be fully compatibale, or I must fail
    if (config_flags & BIN_WRITE_CFG_ALWAYS_CREATE_NEW || file_size == 0)
        ASSERT_NO_RET_FALSE(1101,InitEmptyFile());
    else
        ASSERT_NO_RET_FALSE(1102,CheckExistingFile(file_size));

    initialized = true;
    return true;
}

bool BW_plugin::Finalize()
{
    if (fd != -1)
    {
        if (close(fd) < 0)
            ERRNO_SHOW(1046,"close",filename);
        fd = -1;
    }

    if (this->pool)
    {
        if (munmap(pool,max_pool_size) != 0)
            ERRNO_SHOW(1047,"munmap",filename);
        pool = nullptr;
    }

    this->initialized = false;
    this->check_only = false;
    this->check_failures = 0;
    return true;
}

bool BW_plugin::InitEmptyFile()
{
// setFileSize?? --> this function can be called at differrent situations
    ASSERT_NO_RET_FALSE(1104,fd >= 0);
    if (ftruncate(fd,BW2_INITIAL_FILE_SIZE) < 0)
    {
        ERRNO_SHOW(1105,"ftruncate",filename);
        return false;
    }
    ASSERT_NO_RET_FALSE(1106,pool != nullptr);
    memset(pool,0,sizeof(*pool));
    
    STRCPY(pool->binary_xml_write_type_info,"binary_xml.pksoft.org");   // TODO: I want to redirect to github page
    pool->pool_format_version = BIN_WRITE_POOL_FORMAT_VERSION;  
    pool->file_size = BW2_INITIAL_FILE_SIZE;                     
    pool->mmap_size = max_pool_size;

    pool->allocator = sizeof(*pool);
    pool->allocator_limit = pool->file_size;

    pool->tags.max_id = -1;
    pool->params.max_id = -1;
    
    pool->size = pool->allocator;

    this->file_size = pool->file_size;

    return true;
}

bool BW_plugin::CheckExistingFile(int file_size)
{
    ASSERT_NO_RET_FALSE(1953,strcmp(pool->binary_xml_write_type_info,"binary_xml.pksoft.org") == 0);
    ASSERT_NO_RET_FALSE(1954,pool->pool_format_version == BIN_WRITE_POOL_FORMAT_VERSION);
    ASSERT_NO_RET_FALSE(1943,(int)pool->file_size == file_size);
    ASSERT_NO_RET_FALSE(1944,(int)pool->mmap_size == max_pool_size); // !! must be the same !!
    ASSERT_NO_RET_FALSE(1945,pool->allocator_limit == (int)pool->file_size);
    ASSERT_NO_RET_FALSE(1946,pool->allocator >= (int)sizeof(*pool));
    ASSERT_NO_RET_FALSE(1947,pool->allocator <= pool->allocator_limit);

    this->check_only = true;
    this->check_failures = 0;

    if (!pool->tags.check(pool)) // DO NOT OPEN HERE - not ready to write yet
        pool->tags.index = 0; // ASSERT_NO_RET_FALSE(2110,pool->tags.Open(pool));
    if (!pool->params.check(pool)) // DO NOT OPEN HERE - not ready to write yet
        pool->params.index = 0; // ASSERT_NO_RET_FALSE(2111,pool->params.Open(pool));

//    ASSERT_NO_RET_FALSE(1951,allRegistered()); // make tables and be ready for checking of registration
    // check pool->tags
    // check pool->params

    return true;

#if 0
// TODO: I would like enable some cooperation of multiple writers, but it needs exactly the same symbol tables and some locking
    memset(pool,0,file_size);
    return InitEmptyFile();
//    ASSERT_NO_RET_FALSE(1103,NOT_IMPLEMENTED);
#endif
}

bool BW_plugin::makeSpace(int size)
{
    int new_size = ((pool->allocator+4095+size) >> 12 << 12); // I want rounded file size
    ASSERT_NO_RET_FALSE(1143,new_size <= (int)pool->mmap_size);
    if (new_size <= (int)pool->file_size) return true; // no grow - ok
    if (ftruncate(fd,new_size) < 0)
    {
        ERRNO_SHOW(1144,"ftruncate",filename);
        return false;
    }
    pool->file_size = new_size;
    pool->allocator_limit = new_size;

    this->file_size = new_size;
    return true;
}

bool BW_plugin::registerTag(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with element names    
// element names starts at offset pool->tags.names_offset and ends at pool->params.offset
{
    ASSERT_NO_RET_FALSE(1107,name != nullptr);
    ASSERT_NO_RET_FALSE(1119,type >= XBT_NULL && type < XBT_LAST);
    if (check_only)
    {
        bool ok1 = (id >= 0 && id <= pool->tags.max_id);
        const char *name_stored = pool->getTagName(id);
        bool ok2 = name_stored != nullptr && strcmp(name,name_stored) == 0;
        XML_Binary_Type type_stored = pool->getTagType(id);
        bool ok3 = (type_stored == type);
        
        if (ok1 && ok2 && ok3) return true; // OK
        LOG_ERROR("registerTag(%d,%s,%s) failed - %s/%s",id,name,XML_BINARY_TYPE_NAMES[type],name_stored,XML_BINARY_TYPE_NAMES[type_stored]);
        check_failures++; 
        return false;
    }
    ASSERT_NO_RET_NULL(2108,pool->tags.Open(pool)); // must be opened
// elements must be defined first
//    ASSERT_NO_RET_FALSE(1108,pool->params.names_offset == 0);

// root must be later
//    ASSERT_NO_RET_FALSE(1109,pool->root == 0);
//-----------------------------------------------
    if (pool->tags.names_offset == 0) // empty
        pool->tags.names_offset = pool->allocator;

    MAXIMIZE(pool->tags.max_id,id);
    int len = strlen(name);

// allocation
    int size = sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1;
    char *dst = pool->allocate(size); // ID:word|Type:byte|string|NUL:byte
    ASSERT_NO_RET_FALSE(1121,dst != 0);
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
    ROUND32UP(size);
    ASSERT_NO_RET_FALSE(2115,dst+size == reinterpret_cast<char*>(pool)+pool->allocator);
    return true;
}

bool BW_plugin::registerAttr(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with attribute names    
{
    ASSERT_NO_RET_FALSE(1114,name != nullptr);
    ASSERT_NO_RET_FALSE(1120,type >= XBT_NULL && type < XBT_LAST);
    if (check_only)
    {
        bool ok1 = (id >= 0 && id <= pool->params.max_id);
        const char *name_stored = pool->getAttrName(id);
        bool ok2 = name_stored != nullptr && strcmp(name,name_stored) == 0;
        XML_Binary_Type type_stored = pool->getAttrType(id);
        bool ok3 = (type_stored == type);
        
        if (ok1 && ok2 && ok3) return true; // OK
        LOG_ERROR("registerAttr(%d,%s,%s) failed - %s/%s",id,name,XML_BINARY_TYPE_NAMES[type],name_stored,XML_BINARY_TYPE_NAMES[type_stored]);
        check_failures++; 
        return false;
    }
    ASSERT_NO_RET_NULL(2098,pool->params.Open(pool)); // must be opened

// elements must be defined first, empty element are not allowed!
    ASSERT_NO_RET_FALSE(1115,pool->tags.names_offset != 0);
// root must be later
    ASSERT_NO_RET_FALSE(1116,pool->root == 0);
//-----------------------------------------------
    if (pool->params.names_offset == 0)
        pool->params.names_offset = pool->allocator;

    MAXIMIZE(pool->params.max_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
    ASSERT_NO_RET_FALSE(1122,dst != 0);
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
    return true;
}

XML_Tag_Names   BW_plugin::registerTag(const char *name,XML_Binary_Type type)
{
    if (pool->tags.names_offset == 0) // empty
        pool->tags.names_offset = pool->allocator;

    int id;
    if ((pool->tags.flags & BW_SYMBOLS_FAST_REG) == 0)
    {
        BW_offset_t item_offset;
        XML_Binary_Type item_type;

        id = pool->tags.getByName(pool,name,&item_offset,&item_type);
        if (id >= 0) // found
        {
            if (item_type != type) // extending type
            {
                char *POOL = reinterpret_cast<char*>(pool);
                POOL[item_offset+2] = XBT_JoinTypes(type,item_type);
            }   
            return static_cast<XML_Tag_Names>(id);
        }
    }
    ASSERT_NO_RET_(2099,pool->tags.Open(pool),XTNR_NULL); // must be opened

    int len = strlen(name);
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
    ASSERT_NO_RET_(1972,dst != 0,XTNR_NULL);
    id = ++pool->tags.max_id;
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
    return static_cast<XML_Tag_Names>(id);
}

XML_Param_Names BW_plugin::registerParam(const char *name,XML_Binary_Type type)
{
    if (pool->params.names_offset == 0) // empty
        pool->params.names_offset = pool->allocator;

    int id;
    if ((pool->params.flags & BW_SYMBOLS_FAST_REG) == 0)
    {
        BW_offset_t item_offset;
        XML_Binary_Type item_type;

        id = pool->params.getByName(pool,name,&item_offset,&item_type);
        if (id >= 0) // found
        {
            if (item_type != type) // extending type
            {
                char *POOL = reinterpret_cast<char*>(pool);
                POOL[item_offset+2] = XBT_JoinTypes(type,item_type);
            }   
            return static_cast<XML_Param_Names>(id);
        }
    }
    int len = strlen(name);
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
    ASSERT_NO_RET_(1973,dst != 0,XPNR_NULL);
    id = ++pool->params.max_id;
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
    return static_cast<XML_Param_Names>(id);
}

void BW_plugin::importTags(XB_reader *src)
{
    if (pool->tags.max_id == -1) pool->tags.flags |= BW_SYMBOLS_FAST_REG;
    else pool->tags.flags &= ~BW_SYMBOLS_FAST_REG;

    if (src == nullptr) return; // OK

    for(int i = 0;i < src->tag_symbols.count;i++)
        registerTag(
                src->tag_symbols.getSymbol(i),
                static_cast<XML_Binary_Type>(src->tag_symbols.getType(i)));

    if (pool->tags.max_id == -1) pool->tags.flags |= BW_SYMBOLS_FAST_REG;
    else pool->tags.flags &= ~BW_SYMBOLS_FAST_REG;
}

void BW_plugin::importParams(XB_reader *src)
{
    if (pool->params.max_id == -1) pool->params.flags |= BW_SYMBOLS_FAST_REG;
    else pool->params.flags &= ~BW_SYMBOLS_FAST_REG;
    if (src == nullptr) return; // OK

    for(int i = 0;i < src->param_symbols.count;i++)
        registerParam(src->param_symbols.
            getSymbol(i),
            static_cast<XML_Binary_Type>(src->param_symbols.getType(i)));

    if (pool->params.max_id == -1) pool->params.flags |= BW_SYMBOLS_FAST_REG;
    else pool->params.flags &= ~BW_SYMBOLS_FAST_REG;
}

//-------------------------------------------------------------------------------------------------

bool BW_plugin::allRegistered()
{
    ASSERT_NO_RET_FALSE(1142,makeSpace(BW2_INITIAL_FILE_SIZE));
    BW_offset_t names_limit = pool->allocator;
    if (check_only && pool->tags.index != 0 && pool->params.index != 0)
        return (check_failures == 0); // no problems?

    ASSERT_NO_RET_FALSE(1123,pool->tags.index != 0   || pool->makeTable(pool->tags));
    ASSERT_NO_RET_FALSE(1124,pool->params.index != 0 || pool->makeTable(pool->params));
    return true;
}

void BW_plugin::setRoot(const BW_element* X)
// I want to have system of two root elements
// It is doublebuffer concept, which enables to safely read data from one frozen buffer half while other half is active and is prepared
// I am not sure, whether this concept is useful enough, because cost of this is double memory footprint.

// Situation:
// I have standard xb file:
// |<--------base.xb------------->|
// I am writing to BW plugin
// |<pool|completed_size|tags|params|root ----------------------------->|
// Once upon a time I will take data from BW plugin and write them transcoded to the end of base.xb
// |<--------base.xb------------->|root ----------------------------->|
// and shring pool to it's initial empty size
// |<pool|tags|params|00000000000000000000000000000000000|
// From point of view of writing process is this easy and simple.
// But from readers point of view is this very confusing.
// Problem is, that reader should not break his reading when it is in BW plugin part.
// So reading plugin part and commiting and cleaning plugin part should be exclusive 
{
    ASSERT_NO_RET(1125,X != nullptr);
    ASSERT_NO_RET(1126,pool->root == 0);    // set root is possibly only once till flush
    ASSERT_NO_RET(1141,X->offset >= 0 && X->offset < pool->allocator);
    pool->root = X->offset;
}

void *BW_plugin::getRoot()
{
    ASSERT_NO_RET_NULL(1147,this != nullptr);
    ASSERT_NO_RET_NULL(1148,pool != nullptr);
    if (pool->root == 0) return nullptr;
//    ASSERT_NO_RET_NULL(1183,pool->root != 0);
    return reinterpret_cast<char*>(pool) + pool->root; 
}

bool BW_plugin::Write(BW_element* list)
// This function will write all elements in list
{
// Check correct initial state
    ASSERT_NO_RET_FALSE(1151,bin_xml_creator != nullptr);
    ASSERT_NO_RET_FALSE(1177,bin_xml_creator->src == this);
// For all siblings in list
    for(BW_element *element = list;;)
    {
        ASSERT_NO_RET_FALSE(1152,bin_xml_creator->Append(element));
// mark as written
        element->flags |= BIN_WRITTEN;

        element = BWE(element->next);
        ASSERT_NO_RET_FALSE(1153,element != nullptr);
        if (element == list) break;
    }
// free empty place in BW
    return true;
}

bool BW_plugin::Clear()
// I will clear all data - only symbol table will remain
{
    ASSERT_NO_RET_FALSE(1178,pool != nullptr);
    pool->root = 0; // forget data

    // this is original situation
    int tags_size = pool->tags.getNamesSize(pool);
    int params_size = pool->params.getNamesSize(pool);
    char *tags_src = (char*)BWE(pool->tags.names_offset);
    char *params_src = (char*)BWE(pool->params.names_offset);

    // this is requested situation
    char *tags_dst = reinterpret_cast<char*>(pool+1);
    char *params_dst = tags_dst+tags_size;
    // Is it possible to have situatiation, which cannot be solved by this code?
    // -------[PARAMS----][TAGS------------]
    // Yes - when between old params and new TAGS is not enough space for growth TAGS

    if (tags_src > tags_dst && tags_src < tags_dst + tags_size + params_size)
    { // copy params to new place
        char *params2_src = reinterpret_cast<char*>(pool->allocate(params_size));
        ASSERT_NO_RET_FALSE(2103,params2_src);
        memcpy(params2_src,params_src,params_size);
        params_src = params2_src;
    }
    if (tags_src != tags_dst)
    {
        ASSERT_NO_RET_FALSE(2104,(tags_dst >= params_src + params_size) || (tags_dst + tags_size <= params_src));
        memmove(tags_dst,tags_src,tags_size);
    }
    if (params_src != params_dst)
    {
        ASSERT_NO_RET_FALSE(2105,(params_dst >= tags_src + tags_size) || (params_dst + params_size <= tags_src));
        memmove(params_dst,params_src,params_size);
    }
    pool->tags.names_offset = sizeof(*pool);
    pool->tags.index = 0; // to be constructed
    pool->params.names_offset = sizeof(*pool)+tags_size;
    pool->params.index = 0; // to be constructed

    int old_size = pool->allocator;
    int new_size = pool->params.names_offset + params_size;
    pool->allocator = new_size;
    if (old_size > new_size)
        memset((char*)pool+new_size,0,old_size - new_size); // clear memory

    
    ASSERT_NO_RET_FALSE(2106,pool->makeTable(pool->tags));
    ASSERT_NO_RET_FALSE(2107,pool->makeTable(pool->params));
    return true;
}

int  BW_plugin::getDataSize()
{
    if (pool == nullptr) return 0;
    return pool->allocator - sizeof(*pool) - pool->tags.getNamesSize(pool) - pool->params.getNamesSize(pool);
}

//-------------------------------------------------------------------------------------------------

BW_element* BW_plugin::tag(int16_t id)
{
//    XML_Binary_Type tag_type = pool->getTagType(id);
//    assert(tag_type == XBT_NULL || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1145,makeSpace(BW2_INITIAL_FILE_SIZE));

    BW_element* result = pool->new_element(XBT_NULL);
    ASSERT_NO_RET_NULL(2040,result != nullptr);

    result->init(pool,id,XBT_NULL,BIN_WRITE_ELEMENT_FLAG);

    return result;
}

BW_element* BW_plugin::tagStr(int16_t id,const char *value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1127,tag_type == XBT_STRING || tag_type == XBT_VARIANT);

    int value_len = strlen(value);
    ASSERT_NO_RET_NULL(1146,makeSpace(BW2_INITIAL_FILE_SIZE+value_len));

    BW_element* result = pool->new_element(XBT_STRING,value_len);
    ASSERT_NO_RET_NULL(2041,result != nullptr);
    
    result->init(pool,id,XBT_STRING,BIN_WRITE_ELEMENT_FLAG);

    strcpy(reinterpret_cast<char*>(result+1),value);
    return result;
}

BW_element* BW_plugin::tagHexStr(int16_t id,const char *value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1128,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagBLOB(int16_t id,const void *value,int32_t size)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1935,tag_type == XBT_BLOB || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1955,makeSpace(BW2_INITIAL_FILE_SIZE+size+4));

    BW_element* result = pool->new_element(XBT_BLOB,size);
    ASSERT_NO_RET_NULL(2042,result != nullptr);
    
    result->init(pool,id,XBT_BLOB,BIN_WRITE_ELEMENT_FLAG);

    *reinterpret_cast<int32_t*>(result+1) = size;
    memcpy(reinterpret_cast<char*>(result+1)+4,value,size);
    return result;
}

BW_element* BW_plugin::tagInt32(int16_t id,int32_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1130,tag_type == XBT_INT32 || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1956,makeSpace(BW2_INITIAL_FILE_SIZE+sizeof(int32_t)));

    BW_element* result = pool->new_element(XBT_INT32,0);
    ASSERT_NO_RET_NULL(2043,result != nullptr);
    
    result->init(pool,id,XBT_INT32,BIN_WRITE_ELEMENT_FLAG);

    *reinterpret_cast<int32_t*>(result+1) = value;
    return result;
}

BW_element* BW_plugin::tagUInt32(int16_t id,uint32_t value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1191,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagInt64(int16_t id,int64_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(2030,tag_type == XBT_INT64 || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(2031,makeSpace(BW2_INITIAL_FILE_SIZE+sizeof(int64_t)));

    BW_element* result = pool->new_element(XBT_INT64,0);
    ASSERT_NO_RET_NULL(2044,result != nullptr);
    
    result->init(pool,id,XBT_INT64,BIN_WRITE_ELEMENT_FLAG);

    *reinterpret_cast<int64_t*>(result+1) = value;
    return result;
}


BW_element* BW_plugin::tagUInt64(int16_t id,uint64_t value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1192,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagFloat(int16_t id,float value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1132,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagDouble(int16_t id,double value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1133,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagGUID(int16_t id,const char *value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1134,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagSHA1(int16_t id,const uint8_t *value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1135,tag_type == XBT_SHA1);

    ASSERT_NO_RET_NULL(1958,makeSpace(BW2_INITIAL_FILE_SIZE+4));

    BW_element* result = pool->new_element(XBT_SHA1,0);
    ASSERT_NO_RET_NULL(2045,result != nullptr);
    
    result->init(pool,id,XBT_SHA1,BIN_WRITE_ELEMENT_FLAG);
   
    memcpy(result+1,value,20); 
    return result;
}



BW_element* BW_plugin::tagTime(int16_t id,time_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1957,tag_type == XBT_UNIX_TIME || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(2020,makeSpace(BW2_INITIAL_FILE_SIZE+4));

    BW_element* result = pool->new_element(XBT_UNIX_TIME,0);
    ASSERT_NO_RET_NULL(2046,result != nullptr);
    
    result->init(pool,id,XBT_UNIX_TIME,BIN_WRITE_ELEMENT_FLAG);
    
    *reinterpret_cast<uint32_t*>(result+1) = (uint32_t)value;
    return result;
}

BW_element* BW_plugin::tagTime64(int16_t id,int64_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    if (tag_type != XBT_UNIX_TIME64_MSEC && tag_type != XBT_VARIANT)
    {
        LOG_ERROR("[1959] %s.tag_type = %d = %s and should be XBT_UNIX_TIME64_MSEC or XBT_VARIANT",pool->getTagName(id),tag_type,XBT2STR(tag_type));
        // ASSERT_NO_RET_NULL(1959,tag_type == XBT_UNIX_TIME64_MSEC || tag_type == XBT_VARIANT);
        return nullptr;
    }

    ASSERT_NO_RET_NULL(1960,makeSpace(BW2_INITIAL_FILE_SIZE+8));

    BW_element* result = pool->new_element(XBT_UNIX_TIME64_MSEC,0);
    ASSERT_NO_RET_NULL(2047,result != nullptr);
    
    result->init(pool,id,XBT_UNIX_TIME64_MSEC,BIN_WRITE_ELEMENT_FLAG);
    
    *reinterpret_cast<int64_t*>(result+1) = value;
    return result;
}




BW_element* BW_plugin::tagIPv4(int16_t id,const char *value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1137,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagIPv6(int16_t id,const char *value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1138,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagData(int16_t id,XML_Binary_Data_Ref &data)
{
    ASSERT_NO_RET_NULL(2003,this != nullptr);
    ASSERT_NO_RET_NULL(2002,makeSpace(BW2_INITIAL_FILE_SIZE+8+data.size));

    if (data.type == XBT_NULL)
        return tag(id);

    XML_Binary_Type tag_type = pool->getTagType(id);
    if (tag_type != data.type)
    {
        ASSERT_NO_RET_NULL(2001,data.type == XBT_STRING); // only supported conversion
        char *buffer = reinterpret_cast<char*>(alloca(data.size+64));
        char *p = buffer;
        char *p_limit = buffer + data.size + 64;
        // bool XBT_FromString(const char *src,XML_Binary_Type type,char **_wp,char *limit)
        ASSERT_NO_RET_NULL(2004,XBT_FromString(data.data,tag_type,&p,p_limit));
        data.data = buffer;
        data.size = p - buffer;
    }

    BW_element* result = reinterpret_cast<BW_element*>(pool->allocate8(sizeof(BW_element)+data.size));
    if (result == nullptr) return nullptr; // error message already shown in allocate
    result->init(pool,id,tag_type,BIN_WRITE_ELEMENT_FLAG);
    
    memcpy(result+1,data.data,data.size); // copy content
    
    return result;
}
//-------------------------------------------------------------------------------------------------

BW_element* BW_plugin::CopyPath(XB_reader &xb,XML_Item *root,...)
{
    va_list ap;
    va_start(ap,root);
    BW_element *result = CopyPath_(true,xb,root,ap);
    va_end(ap);
    return result;
}

BW_element* BW_plugin::CopyPathOnly(XB_reader &xb,XML_Item *root,...)
{
    va_list ap;
    va_start(ap,root);
    BW_element *result = CopyPath_(false,xb,root,ap);
    va_end(ap);
    return result;
}

BW_element* BW_plugin::CopyPath_(bool copy_whole_last_node,XB_reader &xb,XML_Item *root,va_list ap)
{
// source root
    if (root == nullptr) return nullptr; // nothing? OK
    ASSERT_NO_RET_NULL(1975,root == xb.getRoot());

// destination root
    BW_element* root2;
    bool copy;
    if (pool->root == 0) // no root element -> create one
    {
        setRoot(root2 = tag(root->name));
        copy = true;
    }
    else
    {
        copy = false;
        
        root2 = BWE(pool->root);
        if (root2->identification == XTNR_TRANSACTION && root->name != XTNR_TRANSACTION)
        {
            if (root2->first_child == 0)
            {
                // content encapsulated in <transaction>
                root2->first_child = tag(root->name)->offset;
                copy = true;
            }
            root2 = BWE(root2->first_child);
        }
        if (!copy)
        {
            ASSERT_NO_RET_NULL(1974,root2->identification == root->name); // root element must be the same
            ASSERT_NO_RET_NULL(1976,root2->EqualKeys(xb,root));
        }
    }


//-----------

    // have I destination root?

    XML_Item   *src = root;
    BW_element *dst = root2;

    for(;;)
    {
        XML_Item *element = va_arg(ap,XML_Item *);
        if (element == nullptr) 
        {
            // Recursive copy of last element
            if (copy)
            {
                if (copy_whole_last_node)
                    ASSERT_NO_RET_NULL(1977,CopyAll(dst,xb,src) != nullptr);
                else
                    ASSERT_NO_RET_NULL(1994,dst->CopyKeys(xb,src) != nullptr);
            }
            break;
        }
        if (copy) ASSERT_NO_RET_NULL(1994,dst->CopyKeys(xb,src) != nullptr);
    //---------------------------------------------------------------------------------------------
        BW_element *element2 = nullptr;
        // 1. Find element in dst
        if (dst->first_child != 0)
        {
            BW_element *child  = BWE(dst->first_child);
            for(;;)
            {
                if (child->EqualKeys(xb,element))
                { // Found!
                    element2 = child;
                    break;
                }
                if (child->next == dst->first_child) break; // finished
                child = BWE(child->next);
            }
        }
        // 2. Create if missing
        if (element2 == nullptr)
        {
            dst->add(element2 = tag(element->name));
            copy = true;
        }
        else copy = false;
        src = element;
        dst = element2;
    }
//-----------

    return dst;
}

BW_element*     BW_plugin::CopyAll(BW_element *dst,XB_reader &xb,XML_Item *src)
{
// this should recursively copy whole subtree of src
// must be empty element
    ASSERT_NO_RET_NULL(2005,dst != nullptr);
    ASSERT_NO_RET_NULL(1997,dst->first_child == 0);
    ASSERT_NO_RET_NULL(1998,dst->first_attribute == 0);
    ASSERT_NO_RET_NULL(1999,dst->identification == (int16_t)src->name); // must be the same element type

    for(int i = 0;i < src->paramcount;i++)
    {
        XML_Param_Description *PD = src->getParamByIndex(i);
        ASSERT_NO_RET_NULL(1984,dst->attrCopy(xb,src,PD) == dst);
    }
    for(int i = 0;i < src->childcount;i++)
    {
        XML_Item *child = src->getChildByIndex(i);
        XML_Binary_Data_Ref data = child->getData();
        BW_element *child2 = CopyAll(tagData(child->name,data),xb,child);
        dst->add(child2);
    }
    return dst;
}

//-------------------------------------------------------------------------------------------------

const char *BW_plugin::getNodeName(void *element)
{
    if (element == nullptr) return "?";
    BW_element *E = reinterpret_cast<BW_element*>(element);
    ASSERT_NO_RET_NULL(1149,reinterpret_cast<char*>(E) - reinterpret_cast<char*>(pool) > 0);
    ASSERT_NO_RET_NULL(1150,reinterpret_cast<char*>(E) - reinterpret_cast<char*>(pool) + sizeof(BW_element) <= pool->file_size);

    if (E->flags & BIN_WRITE_ELEMENT_FLAG) // TAG
        return pool->getTagName(E->identification);
    else    // PARAM
        return pool->getAttrName(E->identification);
}

const char *BW_plugin::getNodeValue(void *element)
{
    BW_element *E = reinterpret_cast<BW_element*>(element);
    return XBT_ToString(static_cast<XML_Binary_Type>(E->value_type),reinterpret_cast<char*>(E+1));
}

const char *BW_plugin::getNodeBinValue(void *element,XML_Binary_Type &type,int &size)
{
    if (element == nullptr) return nullptr; // no attributes
    BW_element *E = reinterpret_cast<BW_element*>(element);
    type = static_cast<XML_Binary_Type>(E->value_type);
    
    if (type == XBT_BLOB || type == XBT_HEX)
        size = *reinterpret_cast<int32_t*>(E+1);
    else if (type == XBT_STRING)
        size = strlen(reinterpret_cast<char*>(E+1));
    else if (type == XBT_NULL)
    {
        size = 0;
        return nullptr;
    }
    else
        size = 0;
    return reinterpret_cast<const char*>(E+1);
}

BW_element* BW_plugin::BWE(BW_offset_t offset)
{
    ASSERT_NO_RET_NULL(1165,offset >= 0);
    ASSERT_NO_RET_NULL(1166,offset+sizeof(BW_element) <= pool->file_size);
    
    return reinterpret_cast<BW_element*>(reinterpret_cast<char*>(pool)+offset);
}

void BW_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
{
    if (parent == nullptr) return; // nothing

    BW_element *E = reinterpret_cast<BW_element*>(parent);
    if (E->first_child == 0) return; // no elements
    BW_element *child = BWE(E->first_child);
    for(;;)
    {
        on_element(child,userdata);
        if (child->next == E->first_child) break; // finished
        child = E->BWE(child->next);
    }
}

void BW_plugin::ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep)
{
    if (parent == nullptr) return; // no attributes
    BW_element *E = reinterpret_cast<BW_element*>(parent);
    on_element(E,userdata,deep);
    if (E->first_child == 0) return; // no elements
    BW_element *child = BWE(E->first_child);
    for(;;)
    {
        ForAllChildrenRecursively(on_element,child,userdata,deep+1);

        if (child->next == E->first_child) break; // finished
        child = BWE(child->next);
    }
}

void BW_plugin::ForAllParams(OnParam_t on_param,void *element,void *userdata)
{
    if (element == nullptr) return; // no attributes

    BW_element *E = reinterpret_cast<BW_element*>(element);
    BW_element *child = BWE(E->first_attribute);
    for(;;)
    {
// typedef void (*OnParam_t)(const char *param_name,const char *param_value,void *element,void *userdata);
        
        on_param(pool->getAttrName(child->identification),getNodeValue(child),element,userdata);

        if (child->next == E->first_attribute) break; // finished
        child = BWE(child->next);
    }
}

bool BW_plugin::ForAllBinParams(OnBinParam_t on_param,void *element,void *userdata)
{
    if (element == nullptr) return true; // nothing
    BW_element *E = reinterpret_cast<BW_element*>(element);
    if (E->first_attribute == 0) return true; // no attributes
    
    BW_element *child = BWE(E->first_attribute);

//    LOG("%p.ForAllBinParams:",(void*)element);

    for(;;)
    {
// typedef void (*OnBinParam_t)(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata);
        
//        LOG("\t%p: %s ... %s",(void*)child,pool->getAttrName(child->identification),XML_BINARY_TYPE_NAMES[child->value_type]);
       
        on_param(pool->getAttrName(child->identification),child->identification,
            static_cast<XML_Binary_Type>(child->value_type),child->BWD(),
            element,userdata);

        if (child->next == E->first_attribute) break; // finished
        child = BWE(child->next);
    }
    
    return true; // implemented
}

int BW_plugin::getSymbolCount(SymbolTableTypes table)
{
    switch (table)
    {
        case SYMBOL_TABLE_NODES:
            return pool->tags.max_id+1;
        case SYMBOL_TABLE_PARAMS:
            return pool->params.max_id+1;
        default:
            return -1;
    }
}

const char *BW_plugin::getSymbol(SymbolTableTypes table,int idx,XML_Binary_Type &type)
{
    switch (table)
    {
        case SYMBOL_TABLE_NODES:
            if (idx <= pool->tags.max_id)
            {
                type = pool->getTagType(idx);
                return pool->getTagName(idx);
            }
            else
                return nullptr;
        case SYMBOL_TABLE_PARAMS:
            if (idx <= pool->params.max_id)
            {
                type = pool->getAttrType(idx);
                if (type == XBT_UNKNOWN) return nullptr;
                return pool->getAttrName(idx);
            }
            else
                return nullptr;
        default:
            return nullptr;
    }
}

//-------------------------------------------------------------------------------------------------
#undef WORK_ID
#define WORK_ID plugin->getFilename()

BW_plugins::BW_plugins()
{
    MEMSET(plugins,0);
    plugins_count = 0;
}

BW_plugins::~BW_plugins()
{
    for(int i = 0;i < plugins_count;i++)
        std::cerr << ANSI_RED_BRIGHT "BW_plugin " << plugins[i]->getFilename() << " was not correctly unregistered!" ANSI_RESET_LF;
}

bool BW_plugins::registerPlugin(BW_plugin *plugin)
{
    ASSERT_NO_RET_FALSE(2009,plugin != nullptr);
    ASSERT_NO_RET_FALSE(2010,plugins_count < MAX_BW_PLUGINS);
    plugins[plugins_count++] = plugin;
    return true;
}

bool BW_plugins::unregisterPlugin(BW_plugin *plugin)
{
    for(int i = 0;i < plugins_count;i++)
        if (plugins[i] == plugin)
        {   
            memmove(plugins+i,plugins+i+1,--plugins_count - i);
            return true;
        }
    return false;
}

BW_plugin *BW_plugins::getPlugin(BW_pool *pool)
{
    for(int i = 0;i < plugins_count;i++)
        if (plugins[i]->getPool() == pool)
            return plugins[i];
    return nullptr;
}

}
#endif // BIN_WRITE_PLUGIN
