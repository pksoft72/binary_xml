#ifndef _BIN_WRITE_PLUGIN_H_
#define _BIN_WRITE_PLUGIN_H_


#ifdef BIN_WRITE_PLUGIN

#include <binary_xml/bin_xml_creator.h>

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

#define BIN_WRITE_POOL_FORMAT_VERSION               0
#define BIN_WRITE_POOL_FORMAT_VERSION_WITH_PARENT   1   
// no first_child & first_attribute but parent & children 
// extended pool info with type guid, doc guid and doc type name


#define BW2_INITIAL_FILE_SIZE       0x4000      // 16kB

#define BIN_WRITE_ATTR_FLAG         0  // element (1) or attribute (0)
#define BIN_WRITE_ELEMENT_FLAG      1  // element (1) or attribute (0)
#define BIN_WRITE_REMOTE_VALUE      2  // for sharing big values with stable pointers ouf of pool
#define BIN_WRITTEN                 4  // this element and all of it's children is written
#define BIN_DELETED                 8
#define BIN_WRITE_LOCK              16 // this element has lock in one of its parameters BLOB, so locking operations should be easy

#define BIN_WRITE_CFG_ALWAYS_CREATE_NEW     1

#define MAX_BW_PLUGINS              32

namespace pklib_xml {

// BW = Bin Write
#define ROUND32UP(ptr) do { (ptr) = (((ptr)+3) >> 2) << 2; } while(0) // round up
#define ROUND64UP(ptr) do { (ptr) = (((ptr)+7) >> 3) << 3; } while(0) // round up

typedef int32_t BW_offset_t;                // pool + this value -> pointer

class BW_element;   // basic element - tag or parameter
class BW_pool;      // header of whole allocated memory
class BW_plugin;    // service object

//-------------------------------------------------------------------------------------------------
#define BW_SYMBOLS_FAST_REG 1
class BW_symbol_table_16B
{
public:
    int32_t         max_id;                 // maximum defines sizes of indexes
    BW_offset_t     names_offset;           // elements table link
    BW_offset_t     index;                  // BW_offset_t[id]
    uint32_t        flags;                  // BW_SYMBOLS_FAST_REG | ....

    int16_t         getByName(BW_pool *pool,const char *name,BW_offset_t *offset,XML_Binary_Type *type);
    const char      *getName(BW_pool *pool,int search_id);
    int32_t         getNamesSize(BW_pool *pool);
    bool            check(BW_pool *pool);
    bool            Open(BW_pool *pool);
};

class BW_pool // this is flat pointer-less structure mapped directly to the first position of shared memory
{
public:
    uint32_t                size;                  // useful convention to have size in the first 4 bytes
    char                    binary_xml_write_type_info[24]; // identification of file
    uint32_t                pool_format_version;            // don't misinterpret data!
    uint32_t                file_size;                      // should not allocate beyond file_size!!
    uint32_t                mmap_size;                      // this is whole allocation limit - memory is mapped to this size

    BW_offset_t             reserved;                       // was payload
    BW_offset_t             root;                           // pointer to the first element - it is not always the same, as payload
    uint32_t                master_xb_size;                 // how big master .xb file is finished

    BW_offset_t             allocator;
    BW_offset_t             allocator_limit;                // end of space in which allocation is possible
   
    BW_symbol_table_16B     tags;
    BW_symbol_table_16B     params;

//  pool_format_version = 1
//  uint8_t                 doc_type_id[16]                 // GUID identification of type of document
//  uint8_t                 doc_id[16]                      // GUID identification of this document - for match with newer versions
//  char                    doc_type_name[32]               // some document type name for logging

public: // index tables (indexed by id) - allocated on pool
    int             getPoolSize() const { return (pool_format_version == 0 ? 9*4+24+32 : sizeof(BW_pool)); }
    XML_Binary_Type getTagType(int16_t id);
    const char*     getTagName(int16_t id);

    XML_Binary_Type getAttrType(int16_t id);
    const char*     getAttrName(int16_t id);
    
    bool            makeTable(BW_symbol_table_16B &table);
    bool            checkTable(BW_symbol_table_16B &table,BW_offset_t limit);
    bool            check_id();

    const char *    getDocTypeName();
    BW_element*     BWE(BW_offset_t offset) {return reinterpret_cast<BW_element*>(reinterpret_cast<char *>(this) + offset);};
public:
    char*           allocate(int size);
    char*           allocate8(int size);        // 64 bit aligned value
    BW_element*     new_element(XML_Binary_Type type,int size = 0);


    BW_element*     tag(int16_t id);
    BW_element*     tagInt32(int16_t id,int32_t value);
    BW_element*     tagInt64(int16_t id,int64_t value);
    BW_element*     tagString(int16_t id,const char *value);
    BW_element*     tagTime(int16_t id,time_t value);
    BW_element*     tagSHA1(int16_t id,const uint8_t *value);
    BW_element*     tagAny(int16_t id,XML_Binary_Type type,const char *data,int data_size);
};


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
// TODO: BW_offset_t             parent;    -- this should be populated for greater usability
// TODO: there can be only first_child and attributes would be distinguished via flags - so we could stay in 24B per node

    // value is placed just after this object

    void        init(BW_pool *pool,int16_t identification,int8_t value_type,int8_t flags);

    BW_pool*    getPool();
    BW_element* BWE(BW_offset_t offset);
    char*       BWD();

    inline const char* getName() {  return (flags & BIN_WRITE_ELEMENT_FLAG) ? getPool()->getTagName(identification) : getPool()->getAttrName(identification); };
    inline XML_Binary_Type getSymbolType() {  return (flags & BIN_WRITE_ELEMENT_FLAG) ? getPool()->getTagType(identification) : getPool()->getAttrType(identification); }

    BW_element* join(BW_element *B);    // this will connect two circles
    BW_element* add(BW_element *tag);
    BW_element* remove(BW_element *tag);
    BW_element* replace(BW_element *old_value,BW_element *new_value);


    BW_element  *attrNull(int16_t id);
    BW_element  *attrStr(int16_t id,const char *value);
    BW_element  *attrStr2(int16_t id,const char *beg,const char *end);
    BW_element  *attrHexStr(int16_t id,const char *value);
    BW_element  *attrBLOB(int16_t id,const void *value,int32_t size);
    BW_element  *attrInt32(int16_t id,int32_t value,BW_element **dst_attr = nullptr);
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
    BW_element  *attrData(int16_t id,XML_Binary_Data_Ref &data);
    BW_element  *attrCopy(XB_reader &xb,XML_Item *X,XML_Param_Description *param_desc);
    BW_element  *attrGet(int16_t id);
    
    BW_element  *tagGet(int16_t id);
    BW_element  *tag(int16_t id); // find or create
    BW_element  *tagSetInt32(int16_t id,int32_t value);
    BW_element  *tagSetInt64(int16_t id,int64_t value);
    BW_element  *tagSetString(int16_t id,const char *value);
    BW_element  *tagSetTime(int16_t id,time_t value);
    BW_element  *tagSetSHA1(int16_t id,const uint8_t *value);

    int32_t     *getInt32();
    int64_t     *getInt64();
    uint64_t    *getUInt64();
    char        *getStr();
    XML_Binary_Data_Ref getData();

    BW_element  *findChildByTag(int16_t tag_id);
    BW_element  *findChildByParam(int16_t tag_id,int16_t attr_id,XML_Binary_Type value_type,void *data,int data_size);
    BW_element  *findAttr(int16_t attr_id);
    BW_element*  CopyKeys(XB_reader &xb,XML_Item *src);
    bool         EqualKeys(XB_reader &xb,XML_Item *src);

// !!! BW_FOREACH are 2 commands !!! cannot be used like if () BW_FOREACH2(...)
#define BW_FOREACH2(list,element) BW_offset_t _idx = 0;for(BW_element *element = (list)->NextChild(_idx);element != nullptr;element = (list)->NextChild(_idx))
//#define BW_FOREACH(list,element) for(BW_offset_t _idx = 0,BW_element *element = (list)->NextChild(_idx);element != nullptr;element = (list)->NextChild(_idx))
    BW_element*  NextChild(BW_offset_t &offset);
};

//-------------------------------------------------------------------------------------------------

class BW_plugin : public Bin_src_plugin  // !!! #define BIN_WRITE_PLUGIN
{
// This object organize complete DOM tree built on the fly.
// Can be used to produce xb file via several passes.
// This can be also used to produce open-end xb on incremental base.
// This object creates shared memory which can be read by some other process and data can be ready to publish immediately after write and link.
// This object is not shared so it can have local pointers and also file descriptors to manipulate with file size.
protected:
    int         max_pool_size;  // never exceeed size
    int         fd;             // file handle
    BW_pool     *pool;
    bool        initialized;
    bool        check_only;     // register methods will only check
    int         check_failures; // how many errors was in registration
    uint32_t    config_flags;   // BIN_WRITE_CFG_ALWAYS_CREATE_NEW | ...
public:
    BW_plugin(const char *filename = nullptr,Bin_xml_creator *bin_xml_creator = nullptr,int max_pool_size = 0x20000,uint32_t config_flags = 0);
    virtual ~BW_plugin();

// setFilename & LinkCreator are inherited
    void        setFlags(uint32_t flags);
    bool        setMaxPoolSize(int max_pool_size);
    
    virtual bool Initialize();
        bool InitEmptyFile();
        bool CheckExistingFile(int file_size);
    virtual bool Finalize();

    BW_pool*    getPool() { return pool; }
    bool makeSpace(int size);

    bool registerTag(int16_t id,const char *name,XML_Binary_Type type);
    bool registerAttr(int16_t id,const char *name,XML_Binary_Type type);

    XML_Tag_Names   registerTag(const char *name,XML_Binary_Type type);
    XML_Param_Names registerParam(const char *name,XML_Binary_Type type);

    void importTags(XB_reader *src);
    void importParams(XB_reader *src);
    bool allRegistered();

    void setRoot(const BW_element* X);
    bool Write(BW_element* list);

    bool Clear();
    
    BW_element* BWE(BW_offset_t offset);
    int  getDataSize();
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
    BW_element*     tagSHA1(int16_t id,const uint8_t *value);
    BW_element*     tagTime(int16_t id,time_t value);
    BW_element*     tagTime64(int16_t id,int64_t value);
    BW_element*     tagIPv4(int16_t id,const char *value);
    BW_element*     tagIPv6(int16_t id,const char *value);
    BW_element*     tagData(int16_t id,XML_Binary_Data_Ref &data);
    
    BW_element*     CopyPath(XB_reader &xb,XML_Item *root,...);
    BW_element*     CopyPathOnly(XB_reader &xb,XML_Item *root,...);
    BW_element*     CopyAll(BW_element *dst,XB_reader &xb,XML_Item *src);
private:
    BW_element*     CopyPath_(bool copy_whole_last_node,XB_reader &xb,XML_Item *root,va_list ap);
public: // Bin_src_plugin
    virtual void *getRoot();
    virtual const char *getNodeName(void *element);
    virtual const char *getNodeValue(void *element);
    virtual const char *getNodeBinValue(void *element,XML_Binary_Type &type,int &size);
    virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata);
    virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);
    virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata);
    virtual bool ForAllBinParams(OnBinParam_t on_param,void *element,void *userdata);
    
    virtual int getSymbolCount(SymbolTableTypes table);
    virtual const char *getSymbol(SymbolTableTypes table,int idx,XML_Binary_Type &type);
    
};

} // namespace pklib_xml

#endif // BIN_WRITE_PLUGIN
#endif
