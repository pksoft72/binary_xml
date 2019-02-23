#include "bin_xml_creator.h"

#define BIN_WRITE_ELEMENT_FLAG  1   // element (1) or attribute (0)
#define BIN_WRITE_REMOTE_VALUE  2  // for sharing big values with stable pointers ouf of pool

namespace pklib_xml {
class Bin_write_element
{
protected:
    uint16_t    type;
    uint16_t    identification;
    uint16_t    flags;

    int32_t     first_child_offset;
    int32_t     last_child_offset;
    int32_t     first_attribute_offset;
    int32_t     last_attribute_offset;

    int32_t     next;

    int32_t     value; // on this position can be any data

};

class Bin_write_plugin : public Bin_src_plugin
{
private:
    int  pool_size;
    char *pool;         // preallocated memory pool - all data in pool are referenced relatively (with exception BIN_WRITE_REMOTE_VALUE)
    char *allocator;    // pointer to the first empty space

    int32_t elements_start;
    int32_t attributes_start;
    int32_t root;

    int32_t max_element_id;
    int32_t max_attribute_id;
public:
    Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator); 
    virtual ~Bin_write_plugin();

    char *allocate(int size);

    void registerElement(int16_t id,const char *name);
    void registerAttribute(int16_t id,const char *name);



};

} // namespace pklib_xml
