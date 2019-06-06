#include "bin_write_plugin.h"
#include "macros.h"
#include <string.h>
#include <assert.h>

namespace pklib_xml {

Bin_write_plugin::Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator)
    :Bin_src_plugin("<internal-write>",bin_xml_creator)
// OK
{
    pool = new char[initial_pool_size];
    pool_size = initial_pool_size;
    memset(pool,0,pool_size);
    allocator = pool; 
    
    elements_start = attributes_start = root_offset = -1;
    elements = attributes = nullptr;
    max_element_id = max_attribute_id = -1;
}

Bin_write_plugin::~Bin_write_plugin()
// OK
{
    if (pool != nullptr)
        delete [] pool;
    pool = nullptr;
}

char *Bin_write_plugin::allocate(int size)
// OK
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
// element names starts at offset elements_start and ends at attributes_start
{
    assert(name != nullptr);
// elements must be defined first
    if (elements_start == -1)
    {
        elements_start = allocator - pool;
    }
    assert(attributes_start == -1);
    assert(root_offset == -1);
    MAXIMIZE(max_element_id,id);
    int len = strlen(name);
    char *dst = allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
    strcpy(dst,name);
}

void Bin_write_plugin::registerAttribute(int16_t id,const char *name,XML_Binary_Type_Stored type)
// I want to fill symbol tables with attribute names    
{
    assert(name != nullptr);
// elements must be defined first
    assert(elements_start != -1);
// root must be later
    assert(root_offset == -1);

    if (attributes_start == -1)
    {
        attributes_start = allocator - pool;
    }
    MAXIMIZE(max_attribute_id,id);
    int len = strlen(name);
    char *dst = allocate(sizeof(int16_t)+sizeof(XML_Binary_Type_Stored)+len+1);
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
    *reinterpret_cast<XML_Binary_Type_Stored*>(dst) = type;
    dst += sizeof(XML_Binary_Type_Stored);
    strcpy(dst,name);
}

void Bin_write_plugin::setRoot(Bin_write_element *X)
{
    assert(root_offset == -1);     // it is possible only once
    assert((char*)X >= pool && (char*)X < pool+pool_size);
    assert(X != nullptr);
    assert(X->owner == this);  // cannot accept other's elements
    makeTable(&elements,max_element_id,elements_start,attributes_start);
    makeTable(&attributes,max_attribute_id,attributes_start,allocator - pool);
    root_offset = ((char*)X - pool);        // use relative pointer and set is as a root
}

void Bin_write_plugin::makeTable(int32_t **table,int max_id,int32_t start,int32_t end)
{
    assert(table != nullptr);
    assert(*table == nullptr); // not set yet
    assert(max_id == -1 || start != -1);
    if (max_id < 0) return; // empty array
    
    *table = new int32_t[max_id+1];
    memset(*table,0xff,sizeof(int32_t)*(max_id+1)); // 0xffffffff = -1

    while (start < end)
    {
    // TODO:
    }
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
//    else
//        return 
}

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
