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
    allocator = pool; 
    
    elements_start = attributes_start = root = -1;
    max_element_id = max_attribute_id = -1;
}

Bin_write_plugin::~Bin_write_plugin()
{
    if (pool != nullptr)
        delete [] pool;
    pool = nullptr;
    
    
}

char *Bin_write_plugin::allocate(int size)
{
// when not enough space - reallocate
    if (allocator + size - pool > pool_size)
    { // this is slow, but should not occure very often
        char *new_pool = new char[pool_size * 2];
        memcpy(new_pool,pool,pool_size);
        delete [] pool;
        allocator = new_pool + (allocator - pool);
        pool = new_pool;
        pool_size *= 2;
    }
    char *ptr = allocator;
    allocator += (size + 3) & ~3; // round up to 32 bits
    return ptr;
}

void Bin_write_plugin::registerElement(int16_t id,const char *name)
// I want to fill symbol tables with element names    
{
    assert(name != nullptr);
// elements must be defined first
    if (elements_start == -1)
    {
        elements_start = allocator - pool;
    }
    assert(attributes_start == -1);
    assert(root == -1);
    MAXIMIZE(max_element_id,id);
    int len = strlen(name);
    char *dst = allocate(sizeof(int16_t)+len+1);
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
    strcpy(dst,name);
}

void Bin_write_plugin::registerAttribute(int16_t id,const char *name)
// I want to fill symbol tables with attribute names    
{
    assert(name != nullptr);
// elements must be defined first
    assert(elements_start != -1);
// root must be later
    assert(root == -1);

    if (attributes_start == -1)
    {
        attributes_start = allocator - pool;
    }
    MAXIMIZE(max_attribute_id,id);
    int len = strlen(name);
    char *dst = allocate(sizeof(int16_t)+len+1);
    *reinterpret_cast<int16_t*>(dst) = id;
    dst += sizeof(int16_t);
    strcpy(dst,name);
}

}
