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
public:
    Bin_write_plugin    *owner; // I need some fixed point in space - owner->pool + XXX_offset makes a pointer

    int16_t     identification; // reference to symbol table for tags/params
    uint16_t    flags;          // BIN_WRITE_ELEMENT_FLAG | BIN_WRITE_REMOTE_VALUE | ... 

    int32_t     next;

    int32_t     first_child_offset;
    int32_t     last_child_offset;
    int32_t     first_attribute_offset;
    int32_t     last_attribute_offset;


    uint16_t    value_type;   // XBT_NULL,...
    int32_t     value; // on this position can be any data - even longer than 32bit
                       // when variable sized value is used (STRING/BLOB ...) size is stored here

    void init(int16_t id,int16_t type,int16_t flags);
};

class Bin_write_tag : public Bin_write_element
{
};

class Bin_write_plugin : public Bin_src_plugin
{
// This object organize complete DOM tree built on the fly.
// Can be use to produce xb file via several passes.
// This can be also used to produce open-end xb on incremental base.
private:
    int  pool_size;
    char *pool;         // preallocated memory pool - all data in pool are referenced relatively (with exception BIN_WRITE_REMOTE_VALUE)
    char *allocator;    // pointer to the first empty space

private:
    int32_t elements_start,*elements;
    int32_t attributes_start,*attributes;
    int32_t root_offset;

    int32_t max_element_id;
    int32_t max_attribute_id;
public:
    Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator); 
    virtual ~Bin_write_plugin();

// allocation 
    char *allocate(int size);

// element registration
    void registerElement(int16_t id,const char *name,XML_Binary_Type_Stored type);
    void registerAttribute(int16_t id,const char *name,XML_Binary_Type_Stored type);

// 
    void setRoot(Bin_write_element *X);
    
    void makeTable(int32_t **table,int max_id,int32_t start,int32_t end);

    Bin_write_element     *tag(int16_t id);
    Bin_write_element     *tagStr(int16_t id,const char *value);
    Bin_write_element     *tagHexStr(int16_t id,const char *value);
    Bin_write_element     *tagBLOB(int16_t id,const char *value,int32_t size);
    Bin_write_element     *tagInt32(int16_t id,int32_t value);
    Bin_write_element     *tagInt64(int16_t id,int64_t value);
    Bin_write_element     *tagFloat(int16_t id,float value);
    Bin_write_element     *tagDouble(int16_t id,double value);
    Bin_write_element     *tagGUID(int16_t id,const char *value);
    Bin_write_element     *tagSHA1(int16_t id,const char *value);
    Bin_write_element     *tagTime(int16_t id,time_t value);
    Bin_write_element     *tagIPv4(int16_t id,const char *value);
    Bin_write_element     *tagIPv6(int16_t id,const char *value);
    
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
