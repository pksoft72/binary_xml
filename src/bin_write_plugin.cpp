#include "bin_write_plugin.h"
#include "macros.h"
#include <string.h>
#include <assert.h>

namespace pklib_xml {

Bin_write_plugin::Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator)
    :Bin_src_plugin("<internal-write>",bin_xml_creator)
{
    pool = new char[initial_pool_size];
    pool_size = initial_pool_size;
    memset(pool,0,pool_size);
    allocator = 0; 
    
    elements_offset = attributes_offset = root_offset = -1;
    elements = attributes = nullptr;
    max_element_id = max_attribute_id = -1;
}

Bin_write_plugin::~Bin_write_plugin()
{
    if (pool != nullptr)
        delete [] pool;
    pool = nullptr;

    if (elements != nullptr)
        delete [] elements;
    elements = nullptr;

    if (attributes != nullptr)
        delete [] attributes;
    attributes = nullptr;
}

BW_offset_t Bin_write_plugin::allocate(int size)
{
// when not enough space - reallocate
    if (allocator + size > pool_size)
    { // this is slow, but should not occure very often
        char *new_pool = new char[pool_size * 2];
        memcpy(new_pool,pool,pool_size);
        memset(new_pool+pool_size,0,pool_size);
        delete [] pool;
        pool = new_pool;
        pool_size *= 2;
    }
    BW_offset_t offset = allocator;
    allocator += (size + 3) & ~3; // round up to 32 bits
    return offset;
}

BW_element_link::BW_element_link(Bin_write_plugin *owner,BW_offset_t offset)
{
    this->owner = owner;
    this->offset = offset;
}


Bin_write_element*  BW_element_link::BWE() const
{
   return owner->BWE(offset); 
}

BW_element_link  Bin_write_plugin::new_element(XML_Binary_Type type,int size)
{
    switch(type)
    {
        case XBT_NULL:
            assert(size == 0);
            break;
        case XBT_VARIABLE:
            assert(type != XBT_VARIABLE); // must be known
            break;
        case XBT_STRING:
            size++; // terminating character
            break;
        case XBT_BINARY:
            size += sizeof(int32_t);
            break;
        case XBT_INT32:
            assert(size == 0);
            size = sizeof(int32_t);
            break;
        case XBT_INT64:
            assert(size == 0);
            size = sizeof(int64_t);
            break;
        case XBT_FLOAT:
            assert(size == 0);
            size = sizeof(float);
            break;
        case XBT_DOUBLE:
            assert(size == 0);
            size = sizeof(double);
            break;
        case XBT_HEX:
            size += sizeof(int32_t);
            break;
        case XBT_GUID:
            assert(size == 0);
            size = 16;
            break;
        case XBT_SHA1:
            assert(size == 0);
            size = 24;
            break;
        case XBT_UNIX_TIME:
            assert(size == 0);
            size = sizeof(int32_t);
            break;
        case XBT_IPv4:
            assert(size == 0);
            size = 4;
            break;
        case XBT_IPv6:
            assert(size == 0);
            size = 16;
            break;
        case XBT_VARIANT:
            assert(type != XBT_VARIANT);
            break;
    }
    BW_element_link result(this,allocate(sizeof(Bin_write_element)+size));
    result.BWE()->value_type = type;
    switch(type)
    {
        case XBT_BINARY:
        case XBT_HEX:
        // size is stored to the 1st byte of data
            *reinterpret_cast<int32_t*>(1+result.BWE()) = size - sizeof(int32_t);
            break;
    }
    return result;
}

Bin_write_element*   Bin_write_plugin::BWE(BW_offset_t offset) const
{ 
    return reinterpret_cast<Bin_write_element*>(pool + offset); 
}

void Bin_write_plugin::registerElement(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with element names    
// element names starts at offset elements_offset and ends at attributes_offset
{
    assert(name != nullptr);
// elements must be defined first
    if (elements_offset == -1)
    {
        elements_offset = allocator;
    }
    assert(attributes_offset == -1);
    assert(root_offset == -1);

    MAXIMIZE(max_element_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool + allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
}

void Bin_write_plugin::registerAttribute(int16_t id,const char *name,XML_Binary_Type type)
// I want to fill symbol tables with attribute names    
{
    assert(name != nullptr);
// elements must be defined first
    assert(elements_offset != -1);
// root must be later
    assert(root_offset == -1);

    if (attributes_offset == -1)
    {
        attributes_offset = allocator;
    }
    assert(root_offset == -1);

    MAXIMIZE(max_attribute_id,id);
    int len = strlen(name);

// allocation
    char *dst = pool + allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
}

void Bin_write_plugin::setRoot(Bin_write_element *X)
{
    assert(root_offset == -1);     // it is possible only once
    assert((char*)X >= pool && (char*)X < pool+pool_size);
    assert(X != nullptr);
    assert(X->owner == this);  // cannot accept other's elements
    root_offset = ((char*)X - pool);        // use relative pointer and set is as a root
    elements = makeTable(max_element_id,elements_offset,attributes_offset);
    attributes = makeTable(max_attribute_id,attributes_offset,root_offset);
}


int32_t * Bin_write_plugin::makeTable(int max_id,int32_t start,int32_t end)
{
    assert(max_id == -1 || start != -1);
    if (max_id < 0) return nullptr; // empty array
    
    int32_t *table = new int32_t[max_id+1];
    memset(table,0xff,sizeof(int32_t)*(max_id+1)); // 0xffffffff = -1

    while (start < end)
    {
        int32_t id = *reinterpret_cast<int32_t*>(pool + start);
        assert(id >= 0 && id <= max_id);
        table[id] = start;
        start += 3 + strlen(pool+start+3)+1;
        start = ((start+3) >> 2) << 2; // round up
    }
    assert(start == end);
    return table;
}

XML_Binary_Type Bin_write_plugin::getTagType(int16_t id)
{
    if (id < 0 || id > max_element_id) return XBT_NULL;
    BW_offset_t offset = elements[id];
}

//-------------------------------------------------------------------------------------------------

BW_element_link Bin_write_plugin::tag(int16_t id)
{
    assert(getTagType(id) ==  XBT_NULL);
    BW_element_link result = this->new_element(XBT_NULL,0);
    result.BWE()->identification = id;
    return result;
}

BW_element_link Bin_write_plugin::tagStr(int16_t id,const char *value)
{
}

BW_element_link Bin_write_plugin::tagHexStr(int16_t id,const char *value)
{
}

BW_element_link Bin_write_plugin::tagBLOB(int16_t id,const char *value,int32_t size)
{
}

BW_element_link Bin_write_plugin::tagInt32(int16_t id,int32_t value)
{
}

BW_element_link Bin_write_plugin::tagInt64(int16_t id,int64_t value)
{
}

BW_element_link Bin_write_plugin::tagFloat(int16_t id,float value)
{
}

BW_element_link Bin_write_plugin::tagDouble(int16_t id,double value)
{
}

BW_element_link Bin_write_plugin::tagGUID(int16_t id,const char *value)
{
}

BW_element_link Bin_write_plugin::tagSHA1(int16_t id,const char *value)
{
}

BW_element_link Bin_write_plugin::tagTime(int16_t id,time_t value)
{
}

BW_element_link Bin_write_plugin::tagIPv4(int16_t id,const char *value)
{
}

BW_element_link Bin_write_plugin::tagIPv6(int16_t id,const char *value)
{
}

//-------------------------------------------------------------------------------------------------

bool Bin_write_plugin::Initialize()
{
    return Bin_src_plugin::Initialize();
}

void *Bin_write_plugin::getRoot()
{
    assert(root_offset != -1);
    return pool + root_offset;
}

const char *Bin_write_plugin::getNodeName(void *element)
{
    if (element == nullptr) return "?";
    Bin_write_element *E = reinterpret_cast<Bin_write_element*>(element);
    assert(E->owner == this);
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
                assert(E->identification <= max_element_id);
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
                assert(E->identification <= max_attribute_id);
                return pool + attributes[E->identification] + 3; // +ID-2B,TYPE-1B
        }
    }
}
// CHECKED----

const char *Bin_write_plugin::getNodeValue(void *element)
{
}

void Bin_write_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
{
}

void Bin_write_plugin::ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep)
{
}

void Bin_write_plugin::ForAllParams(OnParam_t on_param,void *element,void *userdata)
{
}

}
