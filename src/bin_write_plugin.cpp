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

namespace pklib_xml {

void BW_element::init(BW_pool *pool,int16_t identificaton,int8_t value_type,int8_t flags)
{
    assert(sizeof(*this) == 24);
    assert(pool != nullptr);

    int offset = reinterpret_cast<char*>(this) - reinterpret_cast<char*>(pool);
    assert(offset > 0);
    assert(offset+sizeof(BW_element) <= pool->commited_size); 
    this->offset = offset;

    this->identification = identification;
    this->value_type = value_type;
    this->flags = flags;

    this->next = offset;
    this->prev = offset;
    this->first_child = 0;
    this->first_attribute = 0;
}

BW_pool* BW_element::getPool()
{
    return reinterpret_cast<BW_pool*>(reinterpret_cast<char*>(this) - offset);
}

BW_element* BW_element::BWE(BW_offset_t offset)
{
    return reinterpret_cast<BW_element*>(reinterpret_cast<char*>(this) - this->offset + offset);
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
    ASSERT_NO_RET_NULL(1088,attr_type == XBT_INT32 || attr_type == XBT_VARIANT);

    if (attr_type == XBT_INT32)
    {
        BW_element* attr      = pool->new_element(XBT_INT32); // only variable types gives size  --- sizeof(int32_t));
        ASSERT_NO_RET_NULL(1089,attr != nullptr);
        attr->init(pool,id,XBT_INT32,BIN_WRITE_ATTR_FLAG);

        int32_t *dst          = reinterpret_cast<int32_t*>(attr+1); // just after this element
        *dst                  = value; // store value
        
        add(attr);
    }
    else
    {
        ASSERT_NO_RET_NULL(1090,NOT_IMPLEMENTED);
        // TODO: variant
    }

    return this;
}

BW_element*     BW_element::attrInt64(int16_t id,int64_t value)
{
    if (this == nullptr) return nullptr;
        ASSERT_NO_RET_NULL(1091,NOT_IMPLEMENTED); // TODO: variant
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
        ASSERT_NO_RET_NULL(1096,NOT_IMPLEMENTED); // TODO: variant
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
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(reinterpret_cast<char *>(this)+tags.index);
    BW_offset_t offset = elements[id];
    ASSERT_NO_(1053,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(reinterpret_cast<char *>(this) + offset + 2));  
}

const char*     BW_pool::getTagName(int16_t id)
{
    ASSERT_NO_RET_NULL(1064,this != nullptr);
    ASSERT_NO_RET_NULL(1054,tags.index != 0);                // index not initialized 
    ASSERT_NO_RET_NULL(1055,id >=0 && id <= tags.max_id);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(reinterpret_cast<char *>(this)+tags.index);
    BW_offset_t offset = elements[id];
    ASSERT_NO_RET_NULL(1056,offset != 0);                                       // tag id should be correctly registered!
    return reinterpret_cast<const char *>(reinterpret_cast<char *>(this) + offset + 3);  
}

XML_Binary_Type BW_pool::getAttrType(int16_t id)
{
    ASSERT_NO_(1065,this != nullptr,return XBT_UNKNOWN);
    ASSERT_NO_(1057,attrs.index != 0,return XBT_UNKNOWN);                // index not initialized 
    ASSERT_NO_(1058,id >=0 && id <= attrs.max_id,return XBT_UNKNOWN);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(reinterpret_cast<char *>(this)+attrs.index);
    BW_offset_t offset = elements[id];
    ASSERT_NO_(1059,offset != 0,return XBT_UNKNOWN);                                       // tag id should be correctly registered!
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(reinterpret_cast<char *>(this) + offset + 2));  
}

const char*     BW_pool::getAttrName(int16_t id)
{
    ASSERT_NO_RET_NULL(1066,this != nullptr);
    ASSERT_NO_RET_NULL(1060,attrs.index != 0);                // index not initialized 
    ASSERT_NO_RET_NULL(1061,id >=0 && id <= attrs.max_id);    // id out of range - should be in range, because range is defined by writing application
    BW_offset_t *elements = reinterpret_cast<BW_offset_t*>(reinterpret_cast<char *>(this)+attrs.index);
    BW_offset_t offset = elements[id];
    ASSERT_NO_RET_NULL(1062,offset != 0);                                       // tag id should be correctly registered!
    return reinterpret_cast<const char *>(reinterpret_cast<char *>(this) + offset + 3);  
}

bool   BW_pool::makeTable(BW_symbol_table_12B &table)
{
TODO!!!!!!!!
    assert(max_id == -1 || start != -1);
    if (max_id < 0) return nullptr; // empty array
    
    int32_t *table = new int32_t[max_id+1];
    memset(table,0xff,sizeof(int32_t)*(max_id+1)); // 0xffffffff = -1

    while (start < end)
    {
        int id = *reinterpret_cast<int16_t*>(pool + start);
        assert(id >= 0 && id <= max_id);
        table[id] = start;
        start += 3 + strlen(pool+start+3)+1;
        ROUND32UP(start);
    }
    assert(start == end);
    return table;
}


char*  BW_pool::allocate(int size)
{
    if (size <= 0) return 0;
    ASSERT_NO_RET_0(1080,allocator + size <= allocator_limit);

    BW_offset_t offset = allocator;
    allocator += size;
    ROUND32UP(allocator);

    return reinterpret_cast<char*>(this)+offset;
}


BW_element*     BW_pool::new_element(XML_Binary_Type type,int size)
{
    ASSERT_NO_RET_NULL(1067,this != nullptr);
    switch(type)
    {
        case XBT_NULL:
            ASSERT_NO_RET_NULL(1068,size == 0);
            break;
        case XBT_VARIABLE:
            ASSERT_NO_RET_NULL(1069,type != XBT_VARIABLE); // must be known
            break;
        case XBT_STRING:
            size++; // terminating character
            break;
        case XBT_BINARY:
            size += sizeof(int32_t);
            break;
        case XBT_INT32:
            ASSERT_NO_RET_NULL(1070,size == 0);
            size = sizeof(int32_t);
            break;
        case XBT_INT64:
            ASSERT_NO_RET_NULL(1071,size == 0);
            size = sizeof(int64_t);
            break;
        case XBT_FLOAT:
            ASSERT_NO_RET_NULL(1072,size == 0);
            size = sizeof(float);
            break;
        case XBT_DOUBLE:
            ASSERT_NO_RET_NULL(1073,size == 0);
            size = sizeof(double);
            break;
        case XBT_HEX:
            size += sizeof(int32_t);
            break;
        case XBT_GUID:
            ASSERT_NO_RET_NULL(1074,size == 0);
            size = 16;
            break;
        case XBT_SHA1:
            ASSERT_NO_RET_NULL(1075,size == 0);
            size = 24;
            break;
        case XBT_UNIX_TIME:
            ASSERT_NO_RET_NULL(1076,size == 0);
            size = sizeof(int32_t);
            break;
        case XBT_IPv4:
            ASSERT_NO_RET_NULL(1077,size == 0);
            size = 4;
            break;
        case XBT_IPv6:
            ASSERT_NO_RET_NULL(1078,size == 0);
            size = 16;
            break;
        case XBT_VARIANT:
            ASSERT_NO_RET_NULL(1079,type != XBT_VARIANT);
            break;
    }
    BW_element* result = reinterpret_cast<BW_element*>(allocate(sizeof(BW_element)+size));
    if (result == nullptr) return nullptr; // error message already shown in allocate
    result->value_type = type;
    switch(type)
    {
        case XBT_BINARY:
        case XBT_HEX:
        // size is stored to the 1st byte of data
            *reinterpret_cast<int32_t*>(1+result) = size - sizeof(int32_t);
            break;
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

BW2_plugin::BW2_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size)
    : Bin_src_plugin(filename,bin_xml_creator)
{
    this->max_pool_size = max_pool_size;
    this->pool = nullptr;
    this->fd = -1;

}
 
BW2_plugin::~BW2_plugin()
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


bool BW2_plugin::Initialize()
{
// BW2_plugin is owner and the only one (for now) writer of mapped file

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

    return true;
}

bool BW2_plugin::InitEmptyFile()
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
    
    pool->commited_size = sizeof(*pool);
    return true;
}

bool BW2_plugin::CheckExistingFile(int file_size)
{
    ASSERT_NO_RET_FALSE(1103,NOT_IMPLEMENTED);
}

void BW2_plugin::registerTag(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with element names    
// element names starts at offset pool->tags.names_offset and ends at pool->attrs.offset
{
    assert(name != nullptr);
// elements must be defined first
    if (pool->tags.names_offset == -1)
    {
        pool->tags.names_offset = pool->allocator;
    }
    assert(pool->attrs.names_offset == -1);
    assert(pool->roots[0] == 0);
    assert(pool->roots[1] == 0);

    MAXIMIZE(pool->tags.max_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
}

void BW2_plugin::registerAttr(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with attribute names    
{
    assert(name != nullptr);
// elements must be defined first
    assert(pool->tags.names_offset != -1);
// root must be later
    assert(pool->roots[0] == 0);
    assert(pool->roots[1] == 0);

    if (pool->attrs.names_offset == -1)
    {
        pool->attrs.names_offset = pool->allocator;
    }

    MAXIMIZE(pool->attrs.max_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool->allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1); // ID:word|Type:byte|string|NUL:byte
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
}

//-------------------------------------------------------------------------------------------------
















void BW_plugin::setRoot(const BW_element* X)
{
// ::Initialize must be called
    assert(elements != nullptr);
//    assert(attributes != nullptr); -- this can be nullptr

    assert(root == 0);     // it is possible only once
    assert(X.owner == this);
    root = X.names_offset;

}


int32_t * BW_plugin::makeTable(int max_id,int32_t start,int32_t end)
{
}

XML_Binary_Type BW_plugin::getTagType(int16_t id) const
{
    if (id < 0 || id > pool->tags.max_id) return XBT_NULL;
    BW_offset_t offset = elements[id];
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(pool + offset + 2));  
}

XML_Binary_Type BW_plugin::getAttrType(int16_t id) const
{
    if (id < 0 || id > pool->attrs.max_id) return XBT_NULL;
    BW_offset_t offset = attributes[id];
    return static_cast<XML_Binary_Type>(*reinterpret_cast<XML_Binary_Type_Stored*>(pool + offset + 2));  
}

//-------------------------------------------------------------------------------------------------

BW_element* BW_plugin::tag(int16_t id)
{
    assert(getTagType(id) == XBT_NULL);
    BW_element* result = this->new_element(XBT_NULL);
    BW_element      *element = result.BWE();

    element->init(id,XBT_NULL,BIN_WRITE_ELEMENT_FLAG,result.offset);

    return result;
}

BW_element* BW_plugin::tagStr(int16_t id,const char *value)
{
    assert(getTagType(id) == XBT_STRING || getTagType(id) == XBT_VARIANT);
    BW_element* result = this->new_element(XBT_STRING,strlen(value));
    BW_element      *element = result.BWE();
    
    element->init(id,XBT_STRING,BIN_WRITE_ELEMENT_FLAG,result.offset);

    strcpy(reinterpret_cast<char*>(element+1),value);
    return result;
}

BW_element* BW_plugin::tagHexStr(int16_t id,const char *value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagBLOB(int16_t id,const char *value,int32_t size)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagInt32(int16_t id,int32_t value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagInt64(int16_t id,int64_t value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagFloat(int16_t id,float value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagDouble(int16_t id,double value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagGUID(int16_t id,const char *value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagSHA1(int16_t id,const char *value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagTime(int16_t id,time_t value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagIPv4(int16_t id,const char *value)
{
// TODO: not implemented
    assert(false);
}

BW_element* BW_plugin::tagIPv6(int16_t id,const char *value)
{
// TODO: not implemented
    assert(false);
}

//-------------------------------------------------------------------------------------------------

bool BW_plugin::Initialize()
{
// It is possible to call this twice - it must be called before creating tags
    if (elements == nullptr)
        elements = makeTable(pool->tags.max_id,pool->tags.names_offset,pool->attrs.offset);
    if (attributes == nullptr)
        attributes = makeTable(pool->attrs.max_id,pool->attrs.offset,
            (root == 0 ? allocator : root));

    file_size = allocator; // must provide some info about size of file

    return true; // no inherited initialization required, Bin_src_plugin::Initialize would scan file size of "<internal-write>" with error code.
}

void *BW_plugin::getRoot()
{
    assert(root != 0);
    return pool + root;
}

const char *BW_plugin::getNodeName(void *element)
{
    if (element == nullptr) return "?";
    BW_element *E = reinterpret_cast<BW_element*>(element);
    assert(reinterpret_cast<char*>(E) - pool > 0);
    assert(pool + pool_size  - reinterpret_cast<char*>(E) >= sizeof(BW_element));

    if (E->flags & BIN_WRITE_ELEMENT_FLAG) // TAG
    {
        assert(elements != nullptr);
        switch (E->identification)
        {
            case XTNR_NULL: return "NULL";
            case XTNR_META_ROOT: return "meta_root";
            case XTNR_TAG_SYMBOLS: return "tag_symbols";
            case XTNR_PARAM_SYMBOLS: return "param_symbols";
            case XTNR_HASH_INDEX: return "hash_index";
            case XTNR_PAYLOAD: return "payload";
            default:
                assert(E->identification >= 0);
                assert(E->identification <= pool->tags.max_id);
                return pool + elements[E->identification] + 3; // +ID-2B,TYPE-1B
        }
    }
    else    // PARAM
    {
        assert(attributes != nullptr);
        switch (E->identification)
        {
            case XPNR_NULL : return "NULL";
            case XPNR_NAME : return "name";
            case XPNR_HASH : return "hash";
            case XPNR_MODIFIED : return "modified";
            case XPNR_COUNT : return "count";
            case XPNR_FORMAT_VERSION : return "version";
            case XPNR_TYPE_COUNT : return "type_count";

            default:
                assert(E->identification >= 0);
                assert(E->identification <= pool->attrs.max_id);
                return pool + attributes[E->identification] + 3; // +ID-2B,TYPE-1B
        }
    }
}
// CHECKED----

const char *BW_plugin::getNodeValue(void *element)
{
    static char buffer[64];
    BW_element *E = reinterpret_cast<BW_element*>(element);
    switch (E->value_type)
    {
        case XBT_NULL: return nullptr; // empty value
        case XBT_STRING: return reinterpret_cast<char *>(element)+sizeof(BW_element); 
        case XBT_INT32: 
            sprintf(buffer,"%d",*reinterpret_cast<int32_t*>(reinterpret_cast<char *>(element)+sizeof(BW_element)));
            return buffer;
        case XBT_FLOAT:
            sprintf(buffer,"%f",*reinterpret_cast<float*>(reinterpret_cast<char *>(element)+sizeof(BW_element)));
            return buffer;
        default:
            assert(false);
            return nullptr;
    }
    
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
        child = BWE(child->next);
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
        
        on_param(getNodeName(child),getNodeValue(child),element,userdata);

        if (child->next == E->first_attribute) break; // finished
        child = BWE(child->next);
    }
}

}
