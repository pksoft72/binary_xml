#pragma once

#ifdef BIN_WRITE_PLUGIN

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

#define BIN_WRITE_POOL_FORMAT_VERSION   0
#define BW2_INITIAL_FILE_SIZE       0x4000      // 16kB

#define BIN_WRITE_ATTR_FLAG         0  // element (1) or attribute (0)
#define BIN_WRITE_ELEMENT_FLAG      1  // element (1) or attribute (0)
#define BIN_WRITE_REMOTE_VALUE      2  // for sharing big values with stable pointers ouf of pool
#define BIN_WRITTEN                 4  // this element and all of it's children is written

namespace pklib_xml {

// BW = Bin Write
#define ROUND32UP(ptr) do { (ptr) = (((ptr)+3) >> 2) << 2; } while(0) // round up

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
                                                // I can get pointer to 

    int16_t                 identification; // reference to symbol table for tags/params
    uint8_t                 flags;          // BIN_WRITE_ELEMENT_FLAG | BIN_WRITE_REMOTE_VALUE | ... 
    XML_Binary_Type_Stored  value_type;     // 8-bits: XBT_NULL,...

    // round bidirectional list - enables effective adding to last position
    BW_offset_t             next;
    BW_offset_t             prev;

    BW_offset_t             first_child;
    BW_offset_t             first_attribute;

    // value is placed just after this object

    void        init(BW_pool *pool,int16_t identification,int8_t value_type,int8_t flags);

    BW_pool*    getPool();
    BW_element* BWE(BW_offset_t offset);
    char*       BWD();

    BW_element* join(BW_element *B);    // this will connect two circles
    BW_element* add(BW_element *tag);


    BW_element  *attrStr(int16_t id,const char *value);
    BW_element  *attrStr2(int16_t id,const char *beg,const char *end);
    BW_element  *attrHexStr(int16_t id,const char *value);
    BW_element  *attrBLOB(int16_t id,const void *value,int32_t size);
    BW_element  *attrInt32(int16_t id,int32_t value);
    BW_element  *attrUInt32(int16_t id,uint32_t value);
    BW_element  *attrInt64(int16_t id,int64_t value);
    BW_element  *attrUInt64(int16_t id,uint64_t value);
    BW_element  *attrFloat(int16_t id,float value);
    BW_element  *attrDouble(int16_t id,double value);
    BW_element  *attrGUID(int16_t id,const char *value);
    BW_element  *attrSHA1(int16_t id,const char *value);
    BW_element  *attrTime(int16_t id,time_t value);
    BW_element  *attrTime64(int16_t id,int64_t value);
    BW_element  *attrIPv4(int16_t id,const char *value);
    BW_element  *attrIPv6(int16_t id,const char *value);

    BW_element  *attrGet(int16_t id);
    int32_t     *getInt32();
    
    BW_element  *findChildByParam(int16_t tag_id,int16_t attr_id,XML_Binary_Type value_type,void *data,int data_size);
};

class BW_symbol_table_12B
{
public:
    int32_t                 max_id;                 // maximum defines sizes of indexes
    BW_offset_t             names_offset;           // elements table link
    BW_offset_t             index;                  // BW_offset_t[id]
};

class BW_pool // this is flat pointer-less structure mapped directly to the first position of shared memory
{
public:
    uint32_t                size;                  // useful convention to have size in the first 4 bytes
    char                    binary_xml_write_type_info[24]; // identification of file
    uint32_t                pool_format_version;            // don't misinterpret data!
    uint32_t                file_size;                      // should not allocate beyond file_size!!
    uint32_t                mmap_size;                      // this is whole allocation limit - memory is mapped to this size

    BW_offset_t             payload;                        // here begins data
    BW_offset_t             root;                           // pointer to the first element - it is not always the same, as payload
    uint32_t                master_xb_size;                 // how big master .xb file is finished

    BW_offset_t             allocator;
    BW_offset_t             allocator_limit;                // end of space in which allocation is possible
   
    BW_symbol_table_12B     tags;
    BW_symbol_table_12B     attrs;

public: // index tables (indexed by id) - allocated on pool
    XML_Binary_Type getTagType(int16_t id);
    const char*     getTagName(int16_t id);

    XML_Binary_Type getAttrType(int16_t id);
    const char*     getAttrName(int16_t id);
    
    bool            makeTable(BW_symbol_table_12B &table,BW_offset_t limit);
public:
    char*           allocate(int size);
    BW_element*     new_element(XML_Binary_Type type,int size = 0);
};

//-------------------------------------------------------------------------------------------------

class BW_plugin : public Bin_src_plugin
{
// This object organize complete DOM tree built on the fly.
// Can be used to produce xb file via several passes.
// This can be also used to produce open-end xb on incremental base.
// This object creates shared memory which can be read by some other process and data can be ready to publish immediately after write and link.
protected:
    int         max_pool_size;  // never exceeed size
    int         fd;             // file handle
    BW_pool     *pool;
    bool        initialized;
public:
    BW_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size);
    virtual ~BW_plugin();
    
    virtual bool Initialize();
        bool InitEmptyFile();
        bool CheckExistingFile(int file_size);

    bool makeSpace(int size);

    bool registerTag(int16_t id,const char *name,XML_Binary_Type type);
    bool registerAttr(int16_t id,const char *name,XML_Binary_Type type);
    bool allRegistered();

    void setRoot(const BW_element* X);
    bool Write(BW_element* list);

    bool Clear();
    
    BW_element* BWE(BW_offset_t offset);
public: // tag creation
    BW_element*     tag(int16_t id);
    BW_element*     tagStr(int16_t id,const char *value);
    BW_element*     tagHexStr(int16_t id,const char *value);
    BW_element*     tagBLOB(int16_t id,const void *value,int32_t size);
    BW_element*     tagInt32(int16_t id,int32_t value);
    BW_element*     tagUInt32(int16_t id,uint32_t value);
    BW_element*     tagInt64(int16_t id,int64_t value);
    BW_element*     tagUInt64(int16_t id,uint64_t value);
    BW_element*     tagFloat(int16_t id,float value);
    BW_element*     tagDouble(int16_t id,double value);
    BW_element*     tagGUID(int16_t id,const char *value);
    BW_element*     tagSHA1(int16_t id,const char *value);
    BW_element*     tagTime(int16_t id,time_t value);
    BW_element*     tagIPv4(int16_t id,const char *value);
    BW_element*     tagIPv6(int16_t id,const char *value);
public: // Bin_src_plugin
    virtual void *getRoot();
    virtual const char *getNodeName(void *element);
    virtual const char *getNodeValue(void *element);
    virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata);
    virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);
    virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata);
};

} // namespace pklib_xml

#endif // BIN_WRITE_PLUGIN
