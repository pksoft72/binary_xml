#ifdef BIN_WRITE_PLUGIN
#include "bin_write_plugin.h"

#include "bin_xml.h"
#include "bin_xml_types.h"
#include "macros.h"

#include <string.h>
#include <stdio.h>     // perror
#include <unistd.h>    // close,fstat
#include "assert.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h>

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

BW_pool* BW_element::getPool()
{
    return reinterpret_cast<BW_pool*>(THIS - offset);
}

BW_element* BW_element::BWE(BW_offset_t offset)
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

BW_element*     BW_element::attrHexStr(int16_t id,const char *value)
{
    if (this == nullptr) return nullptr;
    if (value == nullptr) return this; // adding null value string is as nothing is added
    ASSERT_NO_RET_NULL(1083,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrBLOB(int16_t id,const char *value,int32_t size)
{
    if (this == nullptr) return nullptr;
    
    BW_pool             *pool = getPool();    
    XML_Binary_Type     attr_type = pool->getAttrType(id);
    ASSERT_NO_RET_NULL(1084,attr_type == XBT_BINARY || attr_type == XBT_VARIANT);
    
    if (attr_type == XBT_STRING)
    {
        BW_element* attr      = pool->new_element(XBT_BINARY,size); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1085,attr != nullptr);

        attr->init(pool,id,XBT_BINARY,BIN_WRITE_ATTR_FLAG);

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
        ASSERT_NO_RET_NULL(1092,NOT_IMPLEMENTED); // TODO: variant
    return this;
}

BW_element*     BW_element::attrDouble(int16_t id,double value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1093,NOT_IMPLEMENTED); // TODO: variant
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

//-------------------------------------------------------------------------------------------------

XML_Binary_Type BW_pool::getTagType(int16_t id) 
{
    ASSERT_NO_(1063,this != nullptr,return XBT_UNKNOWN);
    ASSERT_NO_(1051,tags.index != 0,return XBT_UNKNOWN);                // index not initialized 
    ASSERT_NO_(1052,id >=0 && id <= tags.max_id,return XBT_UNKNOWN);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(THIS+tags.index);
    BW_offset_t offset = elements[id];
    ASSERT_NO_(1053,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(THIS + offset + 2));  
}

const char*     BW_pool::getTagName(int16_t id)
{
    ASSERT_NO_RET_NULL(1064,this != nullptr);
    ASSERT_NO_RET_NULL(1054,tags.index != 0);                // index not initialized 
    if (id < 0)
    {
        switch(id)
        {
            case XTNR_NULL: return "NULL";
            case XTNR_META_ROOT: return "meta_root";
            case XTNR_TAG_SYMBOLS: return "tag_symbols";
            case XTNR_PARAM_SYMBOLS: return "param_symbols";
            case XTNR_HASH_INDEX: return "hash_index";
            case XTNR_PAYLOAD: return "payload";
            case XTNR_REFERENCE: return "reference";
            case XTNR_PROCESSED: return "processed";
            case XTNR_ET_TECERA: return "et-tecera";
        }
    }
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
    ASSERT_NO_(1059,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
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


BW_element*     BW_pool::new_element(XML_Binary_Type type,int size)
{
    ASSERT_NO_RET_NULL(1067,this != nullptr);
    int size2 = XBT_Size(type,size);
    if (size2 < 0) return nullptr;
    BW_element* result = reinterpret_cast<BW_element*>(allocate(sizeof(BW_element)+size2));
    if (result == nullptr) return nullptr; // error message already shown in allocate
    result->value_type = type;
    switch(type)
    {
        case XBT_BINARY:
        case XBT_HEX:
        // size is stored to the 1st byte of data
            *reinterpret_cast<int32_t*>(1+result) = size2 - sizeof(int32_t);
            break;
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

BW_plugin::BW_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size)
    : Bin_src_plugin(filename,bin_xml_creator)
{
    this->max_pool_size = max_pool_size;
    this->pool = nullptr;
    this->fd = -1;
    this->initialized = false;
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

// OK, what 2 do now?
// There are 2 situations - file is empty and I want to create and write ... simple one
//                        - file is somehow populated, it must be fully compatibale, or I must fail
    if (file_size == 0)
        ASSERT_NO_RET_FALSE(1101,InitEmptyFile());
    else
        ASSERT_NO_RET_FALSE(1102,CheckExistingFile(file_size));

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
// TODO: I would like enable some cooperation of multiple writers, but it neets exactly the same symbol tables and some locking
    return InitEmptyFile();
//    ASSERT_NO_RET_FALSE(1103,NOT_IMPLEMENTED);
}

bool BW_plugin::makeSpace(int size)
{
    int new_size = ((pool->allocator+4095) >> 12 << 12) + size;
    ASSERT_NO_RET_FALSE(1143,new_size <= pool->mmap_size);
    if (new_size < pool->file_size) return true; // no grow - ok
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
    return reinterpret_cast<char*>(pool) + pool->root; 
}

bool BW_plugin::Write(BW_element* list)
// This function will write all elements in list
{
// Check correct initial state
    ASSERT_NO_RET_FALSE(1151,bin_xml_creator != nullptr);
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
}
//-------------------------------------------------------------------------------------------------

BW_element* BW_plugin::tag(int16_t id)
{
    assert(pool->getTagType(id) == XBT_NULL);

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

BW_element* BW_plugin::tagBLOB(int16_t id,const char *value,int32_t size)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1129,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagInt32(int16_t id,int32_t value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1130,NOT_IMPLEMENTED);
}

BW_element* BW_plugin::tagInt64(int16_t id,int64_t value)
{
// TODO: not implemented
    ASSERT_NO_RET_NULL(1131,NOT_IMPLEMENTED);
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
// TODO: not implemented
    ASSERT_NO_RET_NULL(1136,NOT_IMPLEMENTED);
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
    static char buffer[64];
    BW_element *E = reinterpret_cast<BW_element*>(element);
    return XBT_ToString(static_cast<XML_Binary_Type>(E->value_type),E+1,buffer,sizeof(buffer));
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

}
#endif // BIN_WRITE_PLUGIN
