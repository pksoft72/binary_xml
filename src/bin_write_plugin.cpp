#ifdef BIN_WRITE_PLUGIN
#include <bin_write_plugin.h>

#include <bin_xml.h>
#include <bin_xml_types.h>
#include <macros.h>

#include <string.h>
#include <stdio.h>     // perror
#include <unistd.h>    // close,fstat
#include "assert.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <assert.h>

#define NOT_IMPLEMENTED false // used in assert
#define THIS reinterpret_cast<char*>(this)

namespace pklib_xml {

void BW_element::init(BW_pool *pool,int16_t identification,int8_t value_type,int8_t flags)
{
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
        ASSERT_NO_RET_NULL(1082,NOT_IMPLEMENTED); // TODO: variant
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

BW_element*     BW_element::attrInt32(int16_t id,int32_t value)
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

    LOG("UInt32: id=%d value=%u",id,value);

    
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
    ASSERT_NO_RET_NULL(0,attr_type == XBT_FLOAT);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1089,attr != nullptr);
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
    ASSERT_NO_RET_NULL(0,attr_type == XBT_DOUBLE);// || attr_type == XBT_VARIANT);

    BW_element* attr      = pool->new_element(attr_type); // only variable types gives size  --- sizeof(int32_t));
    ASSERT_NO_RET_NULL(1089,attr != nullptr);
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

int32_t *BW_element::getInt32()
{
    if (this == nullptr) return nullptr;

    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(identification);
    ASSERT_NO_RET_NULL(1173,attr_type == XBT_INT32);// || attr_type == XBT_VARIANT);

    return reinterpret_cast<int32_t*>(this+1); // just after this element
}

char *BW_element::getStr()
{
    if (this == nullptr) return nullptr;
    if (value_type != XBT_STRING) return nullptr;
    return reinterpret_cast<char*>(this+1);
}

BW_element  *BW_element::findChildByTag(int16_t tag_id)
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
                                                XBT_Size(static_cast<XML_Binary_Type>(attr->value_type),0),
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

//-------------------------------------------------------------------------------------------------

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
    ASSERT_NO_(1057,attrs.index != 0,return XBT_UNKNOWN);                // index not initialized 
    ASSERT_NO_(1058,id >=0 && id <= attrs.max_id,return XBT_UNKNOWN);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+attrs.index);
    BW_offset_t offset = elements[id];

    if (offset == 0) return XBT_UNKNOWN;    // empty id
    //ASSERT_NO_(1059,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(THIS+ offset + 2));  
}

const char*     BW_pool::getAttrName(int16_t id)
{
    ASSERT_NO_RET_NULL(1066,this != nullptr);
    ASSERT_NO_RET_NULL(1060,attrs.index != 0);                // index not initialized 

    switch (id)
    {
        case XPNR_NULL : return "NULL";
        case XPNR_NAME : return "name";
        case XPNR_HASH : return "hash";
        case XPNR_MODIFIED : return "modified";
        case XPNR_COUNT : return "count";
        case XPNR_FORMAT_VERSION : return "version";
        case XPNR_TYPE_COUNT : return "type_count";

        default:
        {
           ASSERT_NO_RET_NULL(1061,id >=0 && id <= attrs.max_id);    // id out of range - should be in range, because range is defined by writing application
           BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+attrs.index);
           BW_offset_t offset = elements[id];
           ASSERT_NO_RET_NULL(1062,offset != 0);                                       // tag id should be correctly registered!
           return reinterpret_cast<const char *>(THIS+ offset + 3);  
        }
    }
}

bool   BW_pool::makeTable(BW_symbol_table_12B &table,BW_offset_t limit)
// This is called after registration of symbols to table - see registerTag & registerAttr
{
    table.index = 0;
    if (table.max_id < 0) return true; // ok, empty array

    ASSERT_NO_RET_FALSE(1111,table.names_offset != 0);
    
    int32_t *index = reinterpret_cast<int32_t*>(allocate(sizeof(int32_t) * (table.max_id+1)));
    ASSERT_NO_RET_FALSE(1118,index != nullptr); // out of memory?
    table.index = reinterpret_cast<char*>(index) - THIS;

    memset(index,0,sizeof(int32_t)*(table.max_id+1)); // 0 means empty

    BW_offset_t start = table.names_offset;
//    BW_offset_t end = reinterpret_cast<char*>(index) - THIS;

    while (start < limit)
    {
        int id = *reinterpret_cast<int16_t*>(THIS + start);
        ASSERT_NO_RET_FALSE(1112,id >= 0 && id <= table.max_id);
        index[id] = start;
        start += 3 + strlen(THIS+start+3)+1;
        ROUND32UP(start);
    }
    ASSERT_NO_RET_FALSE(1113,start == limit);
    return true;
}

bool   BW_pool::checkTable(BW_symbol_table_12B &table,BW_offset_t limit)
{
// I must check, the symbol table is ok and valid!    
    if (table.max_id == -1) return true; // no symbols = OK
    ASSERT_NO_RET_FALSE(1938,table.index >= (int)sizeof(*this));
    ASSERT_NO_RET_FALSE(1939,table.index + (int)sizeof(int32_t) * (table.max_id+1) < payload);
    int32_t *index = reinterpret_cast<int32_t*>(THIS + table.index);
    for(int i = 0;i <= table.max_id;i++)
    {
        if (index[i] == 0) continue; // empty - ok
        ASSERT_NO_RET_FALSE(1940,index[i] >= table.names_offset);
        ASSERT_NO_RET_FALSE(1941,index[i] < table.names_offset);
        
    }
    return true;
}

char*  BW_pool::allocate(int size)
{
    if (size <= 0) return 0;
// memory must be prepared in allocator_limit before allocation - in BW_pool is fd inaccessible
    ASSERT_NO_RET_0(1080,allocator + size <= allocator_limit);

    BW_offset_t offset = allocator;
    allocator += size;
    ROUND32UP(allocator);

    MAXIMIZE(size,allocator);

    return THIS+offset;
}

char*           BW_pool::allocate8(int size)        // 64 bit aligned value
{
    if (size <= 0) return 0;
    ROUND64UP(allocator);
// memory must be prepared in allocator_limit before allocation - in BW_pool is fd inaccessible
    ASSERT_NO_RET_0(1942,allocator + size <= allocator_limit);

    BW_offset_t offset = allocator;
    allocator += size;
    ROUND32UP(allocator);

    MAXIMIZE(size,allocator);

    return THIS+offset;
}

BW_element*     BW_pool::new_element(XML_Binary_Type type,int size)
{
    ASSERT_NO_RET_NULL(1067,this != nullptr);
    int size2 = XBT_Size(type,size);
    if (size2 < 0) return nullptr;
    BW_element* result = reinterpret_cast<BW_element*>(allocate8(sizeof(BW_element)+size2));
    if (result == nullptr) return nullptr; // error message already shown in allocate
    result->value_type = type;
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

//-------------------------------------------------------------------------------------------------

BW_plugin::BW_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size)
    : Bin_src_plugin(nullptr,bin_xml_creator)
// automatic change of extension
{
    this->max_pool_size = max_pool_size;
    this->pool = nullptr;
    this->fd = -1;
    this->initialized = false;
    this->check_only = false;
    this->check_failures = 0;
    

    Bin_src_plugin::setFilename(filename,".xbw"); // will allocate copy of filename
}
 
BW_plugin::~BW_plugin()
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
        if (pool == nullptr)
        {
            ERRNO_SHOW(1049,"mmap",filename);
            return false;
        }
        if (file_size < (int)sizeof(*pool)) file_size = 0; // too small - deleting
        ASSERT_NO_RET_FALSE(1948,file_size == 0 || (int)pool->mmap_size >= file_size); // must be allocated for whole size
        if (file_size == 0) break; // was empty -> OK
        if (pool->mmap_size == max_pool_size) break; // correct size
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
    if (file_size > 0)
        ASSERT_NO_RET_FALSE(1102,CheckExistingFile(file_size));
    else
        ASSERT_NO_RET_FALSE(1101,InitEmptyFile());

    initialized = true;
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
    pool->attrs.max_id = -1;
    
    pool->size = pool->allocator;

    this->file_size = pool->file_size;

    return true;
}

bool BW_plugin::CheckExistingFile(int file_size)
{
    ASSERT_NO_RET_FALSE(1953,strcmp(pool->binary_xml_write_type_info,"binary_xml.pksoft.org") == 0);
    ASSERT_NO_RET_FALSE(1954,pool->pool_format_version == BIN_WRITE_POOL_FORMAT_VERSION);
    ASSERT_NO_RET_FALSE(1943,pool->file_size == file_size);
    ASSERT_NO_RET_FALSE(1944,pool->mmap_size == max_pool_size); // !! must be the same !!
    ASSERT_NO_RET_FALSE(1945,pool->allocator_limit == pool->file_size);
    ASSERT_NO_RET_FALSE(1946,pool->allocator >= sizeof(*pool));
    ASSERT_NO_RET_FALSE(1947,pool->allocator <= pool->allocator_limit);

    this->check_only = true;
    this->check_failures = 0;

//    ASSERT_NO_RET_FALSE(1951,allRegistered()); // make tables and be ready for checking of registration
    // check pool->tags
    // check pool->attrs

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
    int new_size = ((pool->allocator+4095) >> 12 << 12) + size;
    ASSERT_NO_RET_FALSE(1143,new_size <= (int)pool->mmap_size);
    if (new_size < (int)pool->file_size) return true; // no grow - ok
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
// element names starts at offset pool->tags.names_offset and ends at pool->attrs.offset
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
// elements must be defined first
    ASSERT_NO_RET_FALSE(1108,pool->attrs.names_offset == 0);
// root must be later
    ASSERT_NO_RET_FALSE(1109,pool->root == 0);
//-----------------------------------------------
    if (pool->tags.names_offset == 0) // empty
        pool->tags.names_offset = pool->allocator;

    MAXIMIZE(pool->tags.max_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
    ASSERT_NO_RET_FALSE(1121,dst != 0);
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

bool BW_plugin::registerAttr(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with attribute names    
{
    ASSERT_NO_RET_FALSE(1114,name != nullptr);
    ASSERT_NO_RET_FALSE(1120,type >= XBT_NULL && type < XBT_LAST);
    if (check_only)
    {
        bool ok1 = (id >= 0 && id <= pool->attrs.max_id);
        const char *name_stored = pool->getAttrName(id);
        bool ok2 = name_stored != nullptr && strcmp(name,name_stored) == 0;
        XML_Binary_Type type_stored = pool->getAttrType(id);
        bool ok3 = (type_stored == type);
        
        if (ok1 && ok2 && ok3) return true; // OK
        LOG_ERROR("registerAttr(%d,%s,%s) failed - %s/%s",id,name,XML_BINARY_TYPE_NAMES[type],name_stored,XML_BINARY_TYPE_NAMES[type_stored]);
        check_failures++; 
        return false;
    }
// elements must be defined first, empty element are not allowed!
    ASSERT_NO_RET_FALSE(1115,pool->tags.names_offset != 0);
// root must be later
    ASSERT_NO_RET_FALSE(1116,pool->root == 0);
//-----------------------------------------------
    if (pool->attrs.names_offset == 0)
        pool->attrs.names_offset = pool->allocator;

    MAXIMIZE(pool->attrs.max_id,id);
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

bool BW_plugin::allRegistered()
{
    ASSERT_NO_RET_FALSE(1142,makeSpace(BW2_INITIAL_FILE_SIZE));
    if (check_only)
        return (check_failures == 0); // no problems?

    BW_offset_t names_limit = pool->allocator;
    ASSERT_NO_RET_FALSE(1139,pool->payload == 0);
    ASSERT_NO_RET_FALSE(1123,pool->makeTable(pool->tags,pool->attrs.names_offset));
    ASSERT_NO_RET_FALSE(1124,pool->makeTable(pool->attrs,names_limit));
    pool->payload = pool->allocator;
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
// |<pool|completed_size|tags|attrs|root ----------------------------->|
// Once upon a time I will take data from BW plugin and write them transcoded to the end of base.xb
// |<--------base.xb------------->|root ----------------------------->|
// and shring pool to it's initial empty size
// |<pool|tags|attrs|00000000000000000000000000000000000|
// From point of view of writing process is this easy and simple.
// But from readers point of view is this very confusing.
// Problem is, that reader should not break his reading when it is in BW plugin part.
// So reading plugin part and commiting and cleaning plugin part should be exclusive 
{
    ASSERT_NO_RET(1125,X != nullptr);
    ASSERT_NO_RET(1126,pool->root == 0);    // set root is possibly only once till flush
    ASSERT_NO_RET(1140,pool->payload != 0); // allRegistered must be called first
    ASSERT_NO_RET(1141,X->offset >= pool->payload);
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
    ASSERT_NO_RET_FALSE(1179,pool->payload != 0);
    int size2clear = pool->allocator - pool->payload;
    pool->root = 0;
    pool->allocator = pool->payload;
    memset(BWE(pool->allocator),0,size2clear);
    return true; 
}
//-------------------------------------------------------------------------------------------------

BW_element* BW_plugin::tag(int16_t id)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    assert(tag_type == XBT_NULL || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1145,makeSpace(BW2_INITIAL_FILE_SIZE));

    BW_element* result = pool->new_element(XBT_NULL);

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
// TODO: not implemented
    ASSERT_NO_RET_NULL(1131,NOT_IMPLEMENTED);
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

BW_element* BW_plugin::tagSHA1(int16_t id,const char *value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1135,NOT_IMPLEMENTED);
}




BW_element* BW_plugin::tagTime(int16_t id,time_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1957,tag_type == XBT_UNIX_TIME || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1958,makeSpace(BW2_INITIAL_FILE_SIZE+4));

    BW_element* result = pool->new_element(XBT_UNIX_TIME,0);
    
    result->init(pool,id,XBT_UNIX_TIME,BIN_WRITE_ELEMENT_FLAG);
    
    *reinterpret_cast<uint32_t*>(result+1) = (uint32_t)value;
    return result;
}

BW_element* BW_plugin::tagTime64(int16_t id,int64_t value)
{
    XML_Binary_Type tag_type = pool->getTagType(id);
    ASSERT_NO_RET_NULL(1959,tag_type == XBT_UNIX_TIME64_MSEC || tag_type == XBT_VARIANT);

    ASSERT_NO_RET_NULL(1960,makeSpace(BW2_INITIAL_FILE_SIZE+8));

    BW_element* result = pool->new_element(XBT_UNIX_TIME64_MSEC,0);
    
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
    BW_element *E = reinterpret_cast<BW_element*>(element);
    if (E->first_attribute == 0) return; // no attributes
    
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
            return pool->attrs.max_id+1;
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
            if (idx <= pool->attrs.max_id)
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

}
#endif // BIN_WRITE_PLUGIN
