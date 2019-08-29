#pragma once
#include "bin_xml_creator.h"

// Basic idea
// ==========
// 
// This code should be able to create binary xml on the fly like fprintf(f,"<database><files><file name="test.xml">content</file><file name="test.xb">----</file></files></database>")
// TODO: I want to make "open-end" variant of binary XML.
//       It will have fixed symbol tables (known at file create moment)
//       Most objects will be classic XML_Item base fully processed, but one will be open with empty children list.
//       All children of that object will be following and written sequentially, fully specified (with correctly filled children references).

// Advanced idea
// =============
// 
// All this is something like write only database system. It is shareable across multiple threads.
// All data written just into temporary part will be visible for all other processes.
// 

#define BIN_WRITE_ELEMENT_FLAG  1  // element (1) or attribute (0)
#define BIN_WRITE_REMOTE_VALUE  2  // for sharing big values with stable pointers ouf of pool

namespace pklib_xml {

// BW = Bin Write
#define ROUND32UP(ptr) do { (ptr) = ((start+3) >> 2) << 2; } while(0) // round up

typedef int32_t BW_offset_t;                // pool + this value -> pointer

class BW_element;   // basic element - tag or parameter
class BW_pool;      // header of whole allocated memory
class BW_plugin;    // service object

//-------------------------------------------------------------------------------------------------

class BW_element // 24B
{
// This is DOM element - main brick of wall
public:
    BW_offset_t             offset;             // offset of this element - is used to freely access everything only via pointer

    int16_t                 identification; // reference to symbol table for tags/params
    uint8_t                 flags;          // BIN_WRITE_ELEMENT_FLAG | BIN_WRITE_REMOTE_VALUE | ... 
    XML_Binary_Type_Stored  value_type;     // 8-bits: XBT_NULL,...

    // round bidirectional list - enables effective adding to last position
    BW_offset_t             next;
    BW_offset_t             prev;

    BW_offset_t             first_child;
    BW_offset_t             first_attribute;

    // value is placed just after this object

    void init(int16_t id,int8_t type,int8_t flags,BW_offset_t my_offset);
};

class BW_pool // this is flat pointer-less structure mapped directly to the first position of memory
{
    uint32_t                size;   // useful convention to have size in the first 4 bytes
    char                    binary_xml_write_type_info[16]; // identification of file
    BW_offset_t             allocator;
    BW_offset_t             buffers[2]; // double buffer - only 1 is growing - other is read-only
    
    
    
};

class BW2_plugin : public Bin_src_plugin
{
protected:
    int         size;           // current pool allocated
    int         max_pool_size;  // never exceeed size

    BW_pool     *pool;
public:
    BW2_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size);
    virtual ~BW2_plugin();
    
    virtual bool Initialize();
};


//*************************************************************************************************


class BW_element_link // 8/12B
{
private:
    BW_plugin           *owner;
    BW_offset_t         offset;        // pool + this value -> pointer to BW_element
public:
    BW_element_link(BW_plugin *owner,BW_offset_t offset);
    BW_element_link     add(BW_element_link tag);
    BW_element_link     join(BW_element_link Blink);    // this will connect two circles
// returns element
    BW_element*         BWE() const;
    BW_element*         BWE(BW_offset_t offset) const;
// returns direct pointer to data
    char*               BWD() const;
    char*               BWD(BW_offset_t offset) const;

    friend class BW_plugin;
public:
// these methods will add attribute to BW_element_link and returns original BW_element_link
    BW_element_link     attrStr(int16_t id,const char *value);
    BW_element_link     attrHexStr(int16_t id,const char *value);
    BW_element_link     attrBLOB(int16_t id,const char *value,int32_t size);
    BW_element_link     attrInt32(int16_t id,int32_t value);
    BW_element_link     attrInt64(int16_t id,int64_t value);
    BW_element_link     attrFloat(int16_t id,float value);
    BW_element_link     attrDouble(int16_t id,double value);
    BW_element_link     attrGUID(int16_t id,const char *value);
    BW_element_link     attrSHA1(int16_t id,const char *value);
    BW_element_link     attrTime(int16_t id,time_t value);
    BW_element_link     attrIPv4(int16_t id,const char *value);
    BW_element_link     attrIPv6(int16_t id,const char *value);
};

class BW_plugin : public Bin_src_plugin
{
// This object organize complete DOM tree built on the fly.
// Can be used to produce xb file via several passes.
// This can be also used to produce open-end xb on incremental base.
private:
    int32_t     pool_size;
    char        *pool;        // preallocated memory pool - all data in pool are referenced relatively (with exception BIN_WRITE_REMOTE_VALUE)
    BW_offset_t allocator;    // pointer to the first empty space

private: // beginning of parts
    BW_offset_t elements_offset;        // elements table link
    BW_offset_t attributes_offset;      // attributes table link
private: // maximum defines sizes of indexes
    int32_t max_element_id;
    int32_t max_attribute_id;
private: // index tables (indexed by id) - allocated out of pool
    BW_offset_t *elements;
    BW_offset_t *attributes;
protected:    
    int32_t *makeTable(int max_id,int32_t start,int32_t end);
private:
    BW_offset_t root;                   // root document link

// construction
public:
    BW_plugin(int initial_pool_size,Bin_xml_creator *bin_xml_creator); 
// TODO: add memory mapped file variant for parallel access
    virtual ~BW_plugin();

protected: // allocation 
    BW_offset_t          allocate(int size);
    BW_element_link      new_element(XML_Binary_Type type,int size);
public: // translation offset to pointer
    BW_element*   BWE(BW_offset_t offset) const;

public: // element registration
    void registerElement(int16_t id,const char *name,XML_Binary_Type type);
    void registerAttribute(int16_t id,const char *name,XML_Binary_Type type);
    void setRoot(const BW_element_link X);
    
    XML_Binary_Type getTagType(int16_t id) const;
    XML_Binary_Type getAttrType(int16_t id) const;


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

//-------------------------------
    friend class BW_element_link; 
};

} // namespace pklib_xml
