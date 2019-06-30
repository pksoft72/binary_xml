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
class Bin_write_element // 64B
{
// This is DOM element
public:
    Bin_write_plugin        *owner; // I need some fixed point in space - owner->pool + XXX_offset makes a pointer

    int16_t                 identification; // reference to symbol table for tags/params
    uint8_t                 flags;          // BIN_WRITE_ELEMENT_FLAG | BIN_WRITE_REMOTE_VALUE | ... 
    XML_Binary_Type_Stored  value_type;     // 8-bits: XBT_NULL,...

    int32_t                 next;

    int32_t                 first_child_offset;
    int32_t                 last_child_offset;
    int32_t                 first_attribute_offset;
    int32_t                 last_attribute_offset;

    // value is placed just after this object

    void init(int16_t id,int16_t type,int16_t flags);
};

typedef int32_t         BW_offset_t;                // pool + this value -> pointer

// TODO: Big problem
// I would like to have only offset & nothing else (because safety of offset), 
// but with offset only is impossible to access values any internals. For this I need Bin_write_plugin 
// pointer.

class BW_element_link
{
private:
    Bin_write_plugin    *owner;
    BW_offset_t         offset;        // pool + this value -> pointer to Bin_write_element
public:
    BW_element_link(Bin_write_plugin *owner,BW_offset_t offset);
    BW_element_link     add(BW_element_link tag);
    Bin_write_element*  BWE() const;
};

class Bin_write_plugin : public Bin_src_plugin
{
// This object organize complete DOM tree built on the fly.
// Can be use to produce xb file via several passes.
// This can be also used to produce open-end xb on incremental base.
private:
    int32_t     pool_size;
    char        *pool;        // preallocated memory pool - all data in pool are referenced relatively (with exception BIN_WRITE_REMOTE_VALUE)
    BW_offset_t allocator;    // pointer to the first empty space

private: // beginning of parts
    BW_offset_t elements_offset;
    BW_offset_t attributes_offset;
    BW_offset_t root_offset;
private: // maximum defines sizes of indexes
    int32_t max_element_id;
    int32_t max_attribute_id;
private: // index tables (indexed by id) - allocated out of pool
    BW_offset_t *elements;
    BW_offset_t *attributes;

// construction
public:
    Bin_write_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator); 
    virtual ~Bin_write_plugin();

protected: // allocation 
    BW_offset_t          allocate(int size);
    BW_element_link      new_element(XML_Binary_Type type,int size);
public: // translation offset to pointer
    Bin_write_element*   BWE(BW_offset_t offset) const;

public: // element registration
    void registerElement(int16_t id,const char *name,XML_Binary_Type type);
    void registerAttribute(int16_t id,const char *name,XML_Binary_Type type);
    void setRoot(Bin_write_element *X);
    
    XML_Binary_Type getTagType(int16_t id);

protected:    
    int32_t *makeTable(int max_id,int32_t start,int32_t end);

public: // tag creation
    BW_element_link     tag(int16_t id);
    BW_element_link     tagStr(int16_t id,const char *value);
    BW_element_link     tagHexStr(int16_t id,const char *value);
    BW_element_link     tagBLOB(int16_t id,const char *value,int32_t size);
    BW_element_link     tagInt32(int16_t id,int32_t value);
    BW_element_link     tagInt64(int16_t id,int64_t value);
    BW_element_link     tagFloat(int16_t id,float value);
    BW_element_link     tagDouble(int16_t id,double value);
    BW_element_link     tagGUID(int16_t id,const char *value);
    BW_element_link     tagSHA1(int16_t id,const char *value);
    BW_element_link     tagTime(int16_t id,time_t value);
    BW_element_link     tagIPv4(int16_t id,const char *value);
    BW_element_link     tagIPv6(int16_t id,const char *value);
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
