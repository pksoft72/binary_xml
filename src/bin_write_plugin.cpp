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
    allocator = pool; 
    
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

char *Bin_write_plugin::allocate(int size)
{
// when not enough space - reallocate
    if (allocator + size - pool > pool_size)
    { // this is slow, but should not occure very often
        char *new_pool = new char[pool_size * 2];
        memcpy(new_pool,pool,pool_size);
        memset(new_pool+pool_size,0,pool_size);
        delete [] pool;
        allocator = new_pool + (allocator - pool);
        pool = new_pool;
        pool_size *= 2;
    }
    char *ptr = allocator;
    allocator += (size + 3) & ~3; // round up to 32 bits
    return ptr;
}

void Bin_write_plugin::registerElement(int16_t id,const char *name,XML_Binary_Type_Stored type)
// I want to fill symbol tables with element names    
// element names starts at offset elements_offset and ends at attributes_offset
{
    assert(name != nullptr);
// elements must be defined first
    if (elements_offset == -1)
    {
        elements_offset = allocator - pool;
    }
    assert(attributes_offset == -1);
    assert(root_offset == -1);

    MAXIMIZE(max_element_id,id);
    int len = strlen(name);

// allocation
    char *dst = allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
// id
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
// type
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
// name
    strcpy(dst,name);
}

void Bin_write_plugin::registerAttribute(int16_t id,const char *name,XML_Binary_Type_Stored type)
// I want to fill symbol tables with attribute names    
{
    assert(name != nullptr);
// elements must be defined first
    assert(elements_offset != -1);
// root must be later
    assert(root_offset == -1);

    if (attributes_offset == -1)
    {
        attributes_offset = allocator - pool;
    }
    assert(root_offset == -1);

    MAXIMIZE(max_attribute_id,id);
    int len = strlen(name);

// allocation
    char *dst = allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
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
