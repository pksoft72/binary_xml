#ifndef _BIN_XML_H_
#define _BIN_XML_H_


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <binary_xml/bin_xml_types.h>

#ifndef nullptr
#define nullptr NULL
#endif




namespace pklib_xml
{

const int SW_VERSION = 0x200;   // 0.2.0
const int XB_FORMAT_VERSION = 0x00001;  // 0.1
const int XML_MAX_DEEP = 1024;

#define XB_FORMAT_VERSION 0x100
//-------------------------------------------------------------------------------------------------
//typedef char      type_id_t[2];       // identification of object - code characters
typedef int16_t     tag_name_id_t;      // maximum 32K tag names - should be enough, negative values are reserved for metainformation
typedef int16_t     param_name_id_t;    // maximum 32K param names - should be enough, negative values are reserved for metainformation
typedef int32_t     relative_ptr_t;     // +2G..-2G reference ---> limit file size to 2GB


//-------------------------------------------------------------------------------------------------
#define INT32_NULL  -0x8000000

enum XML_Tag_Names
{
    XTNR_MAX        = 0x7fff,   // max code of tag name
    XTNR_NULL       = -1,       // undefined
    XTNR_META_ROOT      = -2,       // meta_root    
    XTNR_TAG_SYMBOLS    = -3,       // tag_symbols
    XTNR_PARAM_SYMBOLS  = -4,       // param_symbols
    XTNR_HASH_INDEX     = -5,       // hash_index
    XTNR_PAYLOAD        = -6,       // payload
    XTNR_REFERENCE      = -7,       // this is reference tag!! - it is exact copy of other tag
    XTNR_PROCESSED      = -8,       // this tag was processed and in .reference field is it's new address
    XTNR_ET_TECERA      = -9,       // this tag is last tag in file and is placeholder for other tags appended to the file later without modification of core
    XTNR_TRANSACTION    = -10,
    XTNR_LAST       = -11        // last
};

enum XML_Param_Names
{
    XPNR_MAX        = 0x7fff,   // maximal code of paramater
    XPNR_NULL       = -1,       // undefined
    XPNR_NAME       = -2,       // name of source file : XBT_STRING
    XPNR_HASH       = -3,       // hash of source file : XBT_SHA1
    XPNR_MODIFIED       = -4,       // modified - date of last write to file : XBT_UNIX_TIME
    XPNR_COUNT      = -5,       // count : XBT_INT32
    XPNR_FORMAT_VERSION = -6,       // version decimal : XBT_INT32
    XPNR_TYPE_COUNT = -7,       // type_count : XBT_INT32
    XPNR_ID         = -8,       // id : XBT_INT32 - this is collision with other - user values
    XPNR_AUTHOR     = -9,       // author : XBT_INT64 - who made transaction
    XPNR_TIME_MS    = -10,      // time_ms : XBT_UNIX_TIME64_MSEC
    XPNR_LAST       = -11
};

//-------------------------------------------------------------------------------------------------
// <meta_root format_version="0x100" name="source.xml" hash="2f32360c988cc76f10f3c0a08759d8813ff3d1c7" modified="2017-01-15 13:38:00">
//   <tag_symbols count="3">....</tag_symbols>
//   <param_symbols count="15">....</param_symbols>
//<!--   <hash_index count="124456">....</hash_index>-->
//   <payload>...</payload>
// </meta_root>
//-------------------------------------------------------------------------------------------------

class XML_Param_Description;
class XB_reader;
class XML_Item;
typedef XML_Item *XML_Item_Array[XML_MAX_DEEP];


class XML_Item 
{
public:
    typedef void (*OnItem_t)(XML_Item *item,void *userdata,int deep);
    typedef void (*OnItemA_t)(XML_Item *item,void *userdata,int deep,XML_Item **root);
public:
    tag_name_id_t   name;
    int16_t     paramcount; // XML_Param_Description[]
    int32_t     childcount; // relative_ptr_t[]
    int32_t     length;     // =0 will signalize infinite object

//--- Manipulation
    bool                Check(XB_reader *R,bool recursive);

    XML_Param_Description *getParamByIndex(int i);
    XML_Param_Description *getParamByName(XML_Param_Names code);

    const relative_ptr_t        *getChildren() const;
    XML_Item          *getChildByIndex(int i);
    XML_Item          *getChildByName(XML_Tag_Names code);

    XML_Item          *getNextChild(XB_reader &R,int &i);    // this method can show even extented xb elements

    void  ForAllChildrenRecursively(OnItem_t on_item,void *userdata,int deep);
    void  ForAllChildrenRecursively_PostOrder(OnItem_t on_item,void *userdata,int deep);
    void  ForAllChildrenRecursively_PostOrderA(OnItemA_t on_item,void *userdata,int deep,XML_Item **root);

//--- Get data values
    const char          *getString() const;
    const int           getStringChunk(int &offset,char *dst,int dst_size) const;
    const void          *getBinary(int32_t *size) const;
    int32_t             getInt() const;         
    const int32_t       *getIntPtr() const;         
    const int64_t       *getInt64Ptr() const;
    XML_Binary_Data_Ref getData();

//--- write as text to stream
    void write(std::ostream& os,XB_reader &R,int deep);
};

class XML_Symbol_Info   // element of symbol table contains name and type of value
{
    relative_ptr_t          name;   // null terminated string
    XML_Binary_Type         type;   // default type of value associated with symbol
};

class XML_ParamName_Info    // element of symbol table for paramnames
{
    relative_ptr_t  name;
};

class XML_Param_Description
{
public:
    param_name_id_t name;   // name o parameter as index to symbol table
    XML_Binary_Type_Stored type;   // here is a nice place for type storing
    uint8_t         reserved;
    relative_ptr_t  data;

    int32_t getInt(const XML_Item *X) const {
        if (this == nullptr) return INT32_NULL;
        if (type == XBT_INT32) return static_cast<int32_t>(data);
        if (X == nullptr) return INT32_NULL;
        if (type == XBT_STRING) return atoi(reinterpret_cast<const char *>(X)+data);
        return INT32_NULL; 
    }
    const int32_t *getIntPtr(const XML_Item *X) const;
    const int32_t *getInt32Ptr(const XML_Item *X,XML_Binary_Type int_type) const;
    const int64_t *getInt64Ptr(const XML_Item *X) const;
    const char *getString(const XML_Item *X) const;
    const int   getStringChunk(const XML_Item *X,int &offset,char *dst,int dst_size) const;

    XML_Binary_Data_Ref getData(XML_Item *X);
};

//-------------------------------------------------------------------------------------------------

struct _XB_symbol_table
{
    XML_Item                  *item_container;
    
    const relative_ptr_t            *reference_array;
    int                             count;

    const XML_Binary_Type_Stored    *type_array;
    int                             type_count;

    _XB_symbol_table();
    virtual ~_XB_symbol_table();

    bool Load(XML_Item *container);
    const char *            getSymbol(tag_name_id_t name_id) const;
    XML_Binary_Type_Stored  getType(tag_name_id_t name_id) const;
    int                     getID(const char *name) const;
};

class XB_reader 
{
public:
    char  *filename;
    size_t      size;
    int     fd; // file descriptor
    XML_Item  *doc;   // doc element -- entire file
    XML_Item  *data;  // user data element

    _XB_symbol_table    tag_symbols;
    _XB_symbol_table    param_symbols;
    int                 verbosity; // this >0 can enable debug messages
public:
    XB_reader(char *filename);
    virtual ~XB_reader();

    bool setFilename(char *filename);  // can set filename later
    void setVerbosity(int verbosity);

    bool Initialize();
    bool Finalize();

    inline const char *getNodeName(tag_name_id_t name_id) const     { return tag_symbols.getSymbol(name_id);}
    inline const char *getParamName(tag_name_id_t name_id) const    { return param_symbols.getSymbol(name_id);}
    inline const char *getFilename() const                          { return filename;}
    inline tag_name_id_t getNodeID(const char *name) const          { return static_cast<tag_name_id_t>(tag_symbols.getID(name));}
    inline param_name_id_t getParamID(const char *name) const       { return static_cast<param_name_id_t>(param_symbols.getID(name));}
    inline XML_Item *getRoot() const                                { return (this == nullptr ? nullptr : data);}
    inline uint32_t getRelPtr(const char *value) const              { return (doc != nullptr && value > reinterpret_cast<char*>(doc) && value < reinterpret_cast<char*>(doc) + size ? (uint32_t)(value - reinterpret_cast<char*>(doc) ) : 0); } 
    inline const char *getString(uint32_t rel_ptr) const            { return (doc != nullptr && rel_ptr != 0 && rel_ptr < size ? (const char *) doc + rel_ptr : nullptr); }

    friend std::ostream& operator<<(std::ostream& os,  XB_reader& R);

    virtual bool ParamIsKey(int param_name) const; 
    virtual bool TagIsKey(int tag_name) const; 
};

std::ostream& operator<<(std::ostream& os, const hash192_t& h);

const char *XTNR2str(int16_t name);
const char *XPNR2str(int16_t name);

} // pklib_xml::
#endif
