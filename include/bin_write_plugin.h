#include "bin_xml_creator.h"

// This code should be able to create binary xml on the fly like fprintf(f,"<database><files><file name="test.xml">content</file><file name="test.xb">----</file></files></database>")
// Task: I want to make "open-end" variant of binary XML.
//       It will have fixed symbol tables (known at file create moment)
//       Most objects will be classic XML_Item base fully processed, but one will be open with empty children list.
//       All children of that object will be following and written sequentially, fully specified (with correctly filled children references).

#define BIN_WRITE_ELEMENT_FLAG  1   // element (1) or attribute (0)
#define BIN_WRITE_REMOTE_VALUE  2  // for sharing big values with stable pointers ouf of pool

namespace pklib_xml {

class Bin_write_plugin;
class Bin_write_element
{
// This is DOM element
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
    int32_t     value_length;

};

class Bin_written_element
{
public:
    Bin_write_plugin *dst;
    int32_t          offset;
public:
    Bin_written_element addTag(int16_t tag_id,uint16_t tag_type);
};

class Bin_write_plugin : public Bin_src_plugin
{
private:
    int  pool_size;
    char *pool;         // preallocated memory pool - all data in pool are referenced relatively (with exception BIN_WRITE_REMOTE_VALUE)
    char *allocator;    // pointer to the first empty space

    int32_t elements_start,*elements;
    int32_t attributes_start,*attributes;
    int32_t root;

    int32_t max_element_id;
    int32_t max_attribute_id;
public:
    Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator); 
    virtual ~Bin_write_plugin();

    char *allocate(int size);

    void registerElement(int16_t id,const char *name,XML_Binary_Type_Stored type);
    void registerAttribute(int16_t id,const char *name,XML_Binary_Type_Stored type);
    void setRoot(Bin_written_element X);
    
    void makeTable(int32_t **table,int max_id,int32_t start,int32_t end);
public: // Bin_src_plugin
    virtual bool Initialize();
    virtual void *getRoot();
    virtual const char *getNodeName(void *element);
    virtual const char *getNodeValue(void *element);
    virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata);
    virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);
    virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata);
};

} // namespace pklib_xml
