#include "bin_xml_creator.h"
#include "bin_xml.h"
#include "files.h"
#include "utils.h"
#include "ANSI.h"
#include "macros.h"

#include <alloca.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h> // qsort_r
#include <stdio.h>
#include <stdarg.h> // va_start ...
#include <limits.h> // PATH_MAX
#include <iostream>

#include "bin_write_plugin.h"

#define DBG(x) x

namespace pklib_xml
{

//-------------------------------------------------------------------------------------------------

Bin_src_plugin::Bin_src_plugin(const char *filename,Bin_xml_creator *bin_xml_creator)
{
    // I need original filename!!! - I cannot convert xml->xb without source filename!
    assert(sizeof(XML_Item) == 12);
    assert(sizeof(XML_Symbol_Info) == 8);
    assert(sizeof(XML_Param_Description) == 8);


    this->filename = nullptr;
    this->bin_xml_creator = bin_xml_creator;
    this->file_size = -1;
    
    setFilename(filename,nullptr);
}

int Bin_src_plugin::getSymbolCount(SymbolTableTypes table)
{
    return -1;
}

const char *Bin_src_plugin::getSymbol(SymbolTableTypes table,int idx,XML_Binary_Type &type)
{
    type = XBT_UNKNOWN;
    return nullptr;
}

void Bin_src_plugin::updateFileSize()
{
    this->file_size = file_getsize(filename);
}

void Bin_src_plugin::setFilename(const char *filename,const char *extension,const char *target_dir)
{
    if (this->filename)
        delete [] this->filename;
    this->filename = AllocFilenameChangeExt(filename,extension,target_dir);
}

void Bin_src_plugin::setFilenameFmt(const char *filename,...)
{
    char expanded[PATH_MAX];
    va_list arglist;
    va_start(arglist,filename);
    vsnprintf(expanded,sizeof(expanded),filename,arglist);
    expanded[PATH_MAX-1] = '\0';
    va_end(arglist);

    setFilename(expanded,nullptr);
}

Bin_src_plugin::~Bin_src_plugin()
{
    if (this->filename)
        delete [] this->filename;
}

void Bin_src_plugin::LinkCreator(Bin_xml_creator *bin_xml_creator)
{
//    assert(this->bin_xml_creator == nullptr);
// it simple redirect creator to some other beast
    this->bin_xml_creator = bin_xml_creator;
}

const char *Bin_src_plugin::getNodeBinValue(void *element,XML_Binary_Type &type,int &size)
{
    type = XBT_UNKNOWN;
    size = 0;
    return nullptr;
}

bool Bin_src_plugin::Initialize()
{
    this->file_size = file_getsize(filename);
    if (this->file_size <= 0)
        fprintf(stderr,ANSI_RED_BRIGHT "Error: file %s is empty!" ANSI_RESET_LF,filename);
    return this->file_size > 0;
}

void *Bin_src_plugin::getRoot()
{
    return nullptr;
}

void Bin_src_plugin::print()
{
    print_deep = 0;
    print_node_counter = 0;
    s_PrintElement(getRoot(),this);
    printf("\n");
}

void Bin_src_plugin::s_PrintElement(void *element,void *userdata)
{
    Bin_src_plugin *_this = reinterpret_cast<Bin_src_plugin*>(userdata);

    printf("\n%*s<%s",_this->print_deep*TAB_SIZE,"",_this->getNodeName(element));
    _this->ForAllParams(s_PrintParam,element,userdata);
    printf(">");

    int current_counter = _this->print_node_counter;
    _this->print_deep++;
    _this->ForAllChildren(s_PrintElement,element,_this);
    _this->print_deep--;
    if (current_counter < _this->print_node_counter)
        printf("\n%*s",_this->print_deep*TAB_SIZE,"");

    const char *value = _this->getNodeValue(element);
    if (value != nullptr)
    {
        printf("%s",value); // TODO: htmlencode
        if (current_counter < _this->print_node_counter)
            printf("\n%*s",_this->print_deep*TAB_SIZE,"");
    }
    printf("</%s>",_this->getNodeName(element));
    _this->print_node_counter++;
}

void Bin_src_plugin::s_PrintParam(const char *param_name,const char *param_value,void *element,void *userdata)
{
    printf(" %s=\"%s\"",param_name,param_value);
}

typedef struct 
{
    int deep;
    int sibling_count;
    Bin_src_plugin *src;
    std::ostream &os;
} BIN_SRC_PLUGIN2STREAM_CONTEXT_t;

void Bin_src_plugin::s_OnBinParam2Stream(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata)
{
    BIN_SRC_PLUGIN2STREAM_CONTEXT_t *CTX = reinterpret_cast<BIN_SRC_PLUGIN2STREAM_CONTEXT_t *>(userdata);

    CTX->os << " " << param_name << "=\"";
    // void XBT_ToStringStream(XML_Binary_Type type,const char *data,std::ostream &os)
    XBT_ToXMLStream(type,param_value,CTX->os);
    CTX->os << "\"";
}

void Bin_src_plugin::s_OnParam2Stream(const char *param_name,const char *param_value,void *element,void *userdata)
{
    BIN_SRC_PLUGIN2STREAM_CONTEXT_t *CTX = reinterpret_cast<BIN_SRC_PLUGIN2STREAM_CONTEXT_t *>(userdata);

    CTX->os << " " << param_name << "=\"";
    for(const char *c = param_value;*c != '\0';c++)
    {
        if (*c == '<') CTX->os << "&lt;";
        else if (*c == '>') CTX->os << "&gt;";
        else if (*c == '&') CTX->os << "&amp;";
        else if (*c == '\"') CTX->os << "&quot;";
        else CTX->os << *c;
    }
    CTX->os << "\"";
}

// typedef void (*OnElementRec_t)(void *element,void *userdata,int deep);
void Bin_src_plugin::s_On2Stream(void *element,void *userdata)
{
    BIN_SRC_PLUGIN2STREAM_CONTEXT_t *CTX = reinterpret_cast<BIN_SRC_PLUGIN2STREAM_CONTEXT_t *>(userdata);

    int deep = CTX->deep;
    int sibling_count = CTX->sibling_count;
//-----------------------------------------------------------------------------------

    if (deep > 0 && sibling_count == 0) // finish parent tag
        CTX->os << ">\n";

    for(int t = 0;t < deep;t++) CTX->os << "\t";


    CTX->os << "<" << CTX->src->getNodeName(element);
    // bool ForAllBinParams(OnBinParam_t on_param,void *element,void *userdata)
    if (!CTX->src->ForAllBinParams(s_OnBinParam2Stream,element,userdata))
    {
        // void Bin_json_plugin::ForAllParams(OnParam_t on_param,void *parent,void *userdata)
        CTX->src->ForAllParams(s_OnParam2Stream,element,userdata);
    }
        
    
    CTX->deep++;
    CTX->sibling_count = 0;

    CTX->src->ForAllChildren(Bin_src_plugin::s_On2Stream,element,userdata);


// value info
    XML_Binary_Type value_type;
    int value_size = 0;
    const char *value = CTX->src->getNodeBinValue(element,value_type,value_size);    // binary value referenced

    if (CTX->sibling_count == 0) 
        if (value == nullptr) CTX->os << "/>\n";
        else
        {
            CTX->os << ">";
            // void XBT_ToStringStream(XML_Binary_Type type,const char *data,std::ostream &os)
            XBT_ToXMLStream(value_type,value,CTX->os);
            CTX->os << "</" << CTX->src->getNodeName(element) << ">\n";
        }
    else
    {
        if (value_type != XBT_NULL)
        {
            for(int t = 0;t < deep+1;t++) CTX->os << "\t";
            XBT_ToXMLStream(value_type,value,CTX->os);

            CTX->os << "\n";    
        }
        for(int t = 0;t < deep;t++) CTX->os << "\t";
        CTX->os << "</" << CTX->src->getNodeName(element) << ">\n";
    }

//-----------------------------------------------------------------------------------
    CTX->deep = deep;
    CTX->sibling_count = sibling_count + 1;
}

std::ostream& operator<<(std::ostream& os, Bin_src_plugin &src)
{
    BIN_SRC_PLUGIN2STREAM_CONTEXT_t context = {0,0,&src,os};
    // void Bin_json_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
    Bin_src_plugin::s_On2Stream(src.getRoot(),(void*)&context);
    return os;
}

//-------------------------------------------------------------------------------------------------

Bin_xml_creator::Bin_xml_creator(const char *src,const char *dst,Bin_xml_creator_target target)
{
    assert(BIN_SRC_PLUGIN_SELECTOR != nullptr);

    this->src = BIN_SRC_PLUGIN_SELECTOR(src,this);
    this->src_allocated = true;
    this->target = target;
    if (target == XBTARGET_XB)
        this->dst = AllocFilenameChangeExt(dst,".xb");
    else
        this->dst = AllocFilenameChangeExt(dst,".xbw");
    this->dst_file = -1; 
    
    this->data = nullptr;


// 2nd pass to fill tables
    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        this->symbol_table[t] = nullptr;
        this->symbol_table_types[t] = nullptr;
        this->symbol_count[t] = 0;
    }
}

Bin_xml_creator::Bin_xml_creator(Bin_src_plugin *src,const char *dst,Bin_xml_creator_target target)
{
    this->src = src;
    this->src->LinkCreator(this); // link it
    this->src_allocated = false;
    if (dst == nullptr) dst = src->getFilename();
    this->target = target;
    if (target == XBTARGET_XB)
        this->dst = AllocFilenameChangeExt(dst,".xb");
    else
        this->dst = AllocFilenameChangeExt(dst,".xbw");
    this->dst_file = -1; 
    this->data = nullptr;
    this->dst_file = -1; 

    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        this->symbol_table[t] = nullptr;
        this->symbol_table_types[t] = nullptr;
        this->symbol_count[t] = 0;
    }
}

Bin_xml_creator::Bin_xml_creator(Bin_src_plugin *src,Bin_xml_creator_target target)
{
    this->src = src;
    this->src->LinkCreator(this); // link it
    this->src_allocated = false;
    this->target = target;
    if (target == XBTARGET_XB)
        this->dst = AllocFilenameChangeExt(dst,".xb");
    else
        this->dst = AllocFilenameChangeExt(dst,".xbw");
    this->dst_file = -1; 
    this->data = nullptr;
    this->dst_file = -1; 

    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        this->symbol_table[t] = nullptr;
        this->symbol_table_types[t] = nullptr;
        this->symbol_count[t] = 0;
    }
}

Bin_xml_creator::~Bin_xml_creator()
{
    if (src_allocated && src != nullptr)
    {
        delete src;
        src = nullptr;
        src_allocated = false;
    }
    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        for(int i = 0;i < symbol_count[t];i++)
        {
            free((void*)symbol_table[t][i]);
        }
        delete [] symbol_table[t];
        delete [] symbol_table_types[t];
        symbol_table[t] = nullptr;
        symbol_table_types[t] = nullptr;
        symbol_count[t] = 0;
    }
    if (dst_file != -1)
    { // dst_file can be closed in destructor
        close(dst_file);
        dst_file = -1; 
    }
    if (dst != nullptr)
        delete [] dst; 
}

//-------------------------------------------------------------------------------------------------

bool Bin_xml_creator::DoAll()
{
    ASSERT_NO_RET_FALSE(2049,dst != nullptr);
    int dst_len = strlen(dst);
    ASSERT_NO_RET_FALSE(2050,dst_len > 0);
    if (dst[dst_len-1] == 'w')
        return Make_xbw();
    else
        return Make_xb();
}

bool Bin_xml_creator::Make_xb()
{
    if (!src->Initialize()) return false;

    // 1. filling symbol tables----------------------
    this->total_node_count = 4; // service elements!
    this->total_param_count = 0;

    void *root = src->getRoot();
    ASSERT_NO_RET_FALSE(1184,root != nullptr);
    // total_node_count, total_param_count
    src->ForAllChildrenRecursively(FirstPassEvent,root,(void*)this,0);

    CopySymbolTable() || MakeSymbolTable();
    // 2. show stats---------------------------------

    DBG(std::cout << "total elements: " << total_node_count << "\n");
    DBG(std::cout << "total params: " << total_param_count << "\n");
    DBG(std::cout << "node names");
    DBG(ShowSymbols(SYMBOL_TABLE_NODES));
    DBG(std::cout << "\nparam names");
    DBG(ShowSymbols(SYMBOL_TABLE_PARAMS));
    DBG(std::cout << "\n");

    // 3. allocate and fill--------------------------
    src->updateFileSize();
    this->data_size_allocated = 1024 + src->getFileSize()*4;
    this->data = reinterpret_cast<char *>(malloc(data_size_allocated)); // some reserve

    DBG(std::cout << "DBG:Buffer: " << (void *)this->data << "\n");
    if (this->data == nullptr)
    {
        std::cerr << "error: " << strerror(errno) << " while malloc(" << src->getFileSize()*2 << ")\n";
        return false;
    }
    this->dst_file_size = FillData();
#ifndef NDEBUG
    // 4. write intermediate uncompressed results to file------
    {
        int dst_file = creat(dst,0644);
        if (dst_file == -1)
        {
            std::cerr << "error: " << strerror(errno) << " while creat(" << dst << ")\n";
            return false;
        }
        AA(dst_file_size);
        int written = write(dst_file,data,dst_file_size);
        assert(written == dst_file_size);

        close(dst_file);
    }
#endif

    this->dst_file_size = Pack();


    // 5. write to file------------------------------
    int dst_file = creat(dst,0644);
    if (dst_file == -1)
    {
        std::cerr << "error: " << strerror(errno) << " while creat(" << dst << ")\n";
        return false;
    }
    AA(dst_file_size);
    int written = 
        write(dst_file,data,dst_file_size);
    assert(written == dst_file_size);

    close(dst_file);


    return (written == dst_file_size);
}

bool Bin_xml_creator::CopySymbolTable() 
{
    // has special function for symbol table?
    if (src->getSymbolCount(SYMBOL_TABLE_NODES) <=  0) return false;
    // copy of symbol tables where src has some - it's faster and no type detection is needed
    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        int count = /*this->symbol_count[t] = */src->getSymbolCount((SymbolTableTypes)t);
        this->symbol_table[t] = reinterpret_cast<const char **>(malloc(sizeof(char*)*count));
        this->symbol_table_types[t] = reinterpret_cast<XML_Binary_Type_Stored*>(malloc(sizeof(XML_Binary_Type_Stored)*count));
        for(int i = 0;i < count;i++)
        {
            XML_Binary_Type type;
            const char *name = src->getSymbol((SymbolTableTypes)t,i,type);
            if (type != XBT_UNKNOWN)
                FindOrAddTyped(name,(SymbolTableTypes)t,type);
        }
    }
    return true;
}

bool Bin_xml_creator::MakeSymbolTable() 
{
    // I don't know final size of array yet, I will allocate more
    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        int count = MAX_SYMBOL_COUNT;//(t == SYMBOL_TABLE_NODES ? total_node_count : total_param_count);
        this->symbol_table[t] = reinterpret_cast<const char **>(alloca(sizeof(char*)*count));
        this->symbol_table_types[t] = reinterpret_cast<XML_Binary_Type_Stored*>(alloca(sizeof(XML_Binary_Type_Stored)*count));
        memset(this->symbol_table_types[t],0,sizeof(XML_Binary_Type_Stored)*count);

        this->symbol_count[t] = 0;
    }

    src->ForAllChildrenRecursively(SecondPassEvent,src->getRoot(),(void*)this,0);

    for(int t = 0;t < SYMBOL_TABLES_COUNT;t++)
    {
        const char **table_backup = this->symbol_table[t];
        XML_Binary_Type_Stored *table_types_backup = this->symbol_table_types[t];

        int size1 = sizeof(char*)*symbol_count[t];
        int size2 = sizeof(XML_Binary_Type_Stored)*symbol_count[t];
        this->symbol_table[t] = reinterpret_cast<const char **>(malloc(size1));
        memcpy(this->symbol_table[t],table_backup,size1);

        this->symbol_table_types[t] = reinterpret_cast<XML_Binary_Type_Stored*>(malloc(size2));
        memcpy(this->symbol_table_types[t],table_types_backup,size2);
    }
    return true;
}

bool Bin_xml_creator::Append(void *element)
{
    if (dst_file == -1)
    {
        dst_file = open(dst,O_WRONLY | O_APPEND); // file must already exist
        if (dst_file < 0)
        {
            std::cerr << "error: " << strerror(errno) << " while appending " << dst << "\n";
            return false;
        }
    }
// I should continue in the same alignement as in original file
    struct stat file_info;
    int err = fstat(dst_file,&file_info);
    if (err != 0)
    {

        ERRNO_SHOW(2051,"fstat",dst);  
        return false;
    }
    int file_align = file_info.st_size & 0xf; // use alignemenet like length of file

    char *wp = data+file_align;
    AA(wp);
    this->WriteNode(&wp,element);
    
    int size = wp - data - file_align;
    int written = write(dst_file,data+file_align,size);
    if (written != size)
    {
            std::cerr << "error: " << strerror(errno) << " cannot append " << size << "B\n";
            return false;
    }
    
    return true;
}

void Bin_xml_creator::FirstPassEvent(void *element,void *userdata,int deep)
{
    assert(element != nullptr);
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->total_node_count++;
    if (!_this->src->ForAllBinParams(FirstPassBinParamEvent,element,userdata)) // this is faster!
        _this->src->ForAllParams(FirstPassParamEvent,element,userdata);
    
}

void Bin_xml_creator::FirstPassBinParamEvent(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata)
{
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->total_param_count++;
}

void Bin_xml_creator::FirstPassParamEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->total_param_count++;
}

void Bin_xml_creator::SecondPassEvent(void *element,void *userdata,int deep)
{
    assert(element != nullptr);
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->FindOrAdd(_this->src->getNodeName(element),SYMBOL_TABLE_NODES,_this->src->getNodeValue(element));
    if (!_this->src->ForAllBinParams(SecondPassBinParamEvent,element,userdata))
        _this->src->ForAllParams(SecondPassParamEvent,element,userdata);
    
}

void Bin_xml_creator::SecondPassBinParamEvent(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata)
{
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->FindOrAddTyped(param_name,SYMBOL_TABLE_PARAMS,type);
}

void Bin_xml_creator::SecondPassParamEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->FindOrAdd(param_name,SYMBOL_TABLE_PARAMS,param_value);
}

//-------------------------------------------------------------------------------------------------

struct MakingXBW_1node
{
// input
    Bin_xml_creator *creator;
    BW_plugin       *bw;
    void            *src_element;
// result
    BW_element      *dst_bw_element;
};

bool Bin_xml_creator::Make_xbw()
{
    if (!src->Initialize()) return false;

    // 1. filling symbol tables----------------------
    this->total_node_count = 4; // service elements!
    this->total_param_count = 0;

    void *root = src->getRoot();
    ASSERT_NO_RET_FALSE(2052,root != nullptr);
    // total_node_count, total_param_count
    src->ForAllChildrenRecursively(FirstPassEvent,root,(void*)this,0);

    CopySymbolTable() || MakeSymbolTable();
    // 2. show stats---------------------------------
    // I have completed 2x2 arrays
    const char              **symbol_table[SYMBOL_TABLES_COUNT];
    XML_Binary_Type_Stored  *symbol_table_types[SYMBOL_TABLES_COUNT];
    int                     symbol_count[SYMBOL_TABLES_COUNT];

    DBG(std::cout << "total elements: " << total_node_count << "\n");
    DBG(std::cout << "total params: " << total_param_count << "\n");
    DBG(std::cout << ANSI_WHITE_HIGH "node names: " ANSI_RESET);
    DBG(ShowSymbols(SYMBOL_TABLE_NODES));
    DBG(std::cout << "\n" ANSI_WHITE_HIGH "param names:" ANSI_RESET);
    DBG(ShowSymbols(SYMBOL_TABLE_PARAMS));
    DBG(std::cout << "\n");

    // 3. allocate and fill--------------------------
    src->updateFileSize();

    // 3.1 create
    BW_plugin W(dst,nullptr,MAX(0x40000,((src->getFileSize() >> 12)+1) << 16)); // 16*more - rounded up to 64kB blocks - should not grow!
    // 3.2 initialize
    ASSERT_NO_RET_FALSE(2053,W.Initialize());
    // 3.3 tags
    for(int i = 0;i < symbol_count[SYMBOL_TABLE_NODES];i++)
        W.registerTag(i,symbol_table[SYMBOL_TABLE_NODES][i],static_cast<XML_Binary_Type>(symbol_table_types[SYMBOL_TABLE_NODES][i]));
    // 3.4 attrs
    for(int i = 0;i < symbol_count[SYMBOL_TABLE_PARAMS];i++)
        W.registerAttr(i,symbol_table[SYMBOL_TABLE_PARAMS][i],static_cast<XML_Binary_Type>(symbol_table_types[SYMBOL_TABLE_PARAMS][i]));
    // 3.5 symbols done
    ASSERT_NO_RET_FALSE(2054,W.allRegistered());

    // 3.6 fill data
    MakingXBW_1node node_info;
    node_info.creator = this;
    node_info.bw      = &W;
    node_info.src_element = root;
    node_info.dst_bw_element = nullptr;

    XStore2XBWEvent(root,&node_info);
    W.setRoot(node_info.dst_bw_element);
//typedef void (*OnElement_t)(void *element,void *userdata);



    return false; // not implemented
}
//-------------------------------------------------------------------------------------------------

int32_t Bin_xml_creator::Pack()
// empty implementation
{
    return dst_file_size;
}
//-------------------------------------------------------------------------------------------------


int32_t Bin_xml_creator::FillData()
{
    char* _wp = data; // write pointer
// ok, co teď? jedu!

    XML_Item *X = reinterpret_cast<XML_Item *>(_wp);
    const char *_x = _wp;
    _wp += sizeof(XML_Item);
    
//  X->type = XML_ITEM_TYPE_ID;
    X->name = XTNR_META_ROOT;
    X->paramcount = 4;  // version,name,hash,modified
    X->childcount = 3;  // tagsymbols,paramsymbols,payload
    //---------------------------
// array of param links
    XML_Param_Description *params = reinterpret_cast<XML_Param_Description*>(_wp);
    _wp += sizeof(XML_Param_Description)*X->paramcount;
// array of children links
    relative_ptr_t *children = reinterpret_cast<relative_ptr_t *>(_wp);
    _wp += sizeof(relative_ptr_t)*X->childcount;
// data
    *(_wp++) = XBT_NULL; // no data
    

// all params
    params[0].name = XPNR_FORMAT_VERSION;
    params[0].type = XBT_INT32;
    params[0].data = XB_FORMAT_VERSION;

    params[1].name = XPNR_NAME;
    params[1].type = XBT_STRING;
    params[1].data = (_wp - _x);
    strcpy(_wp,src->getFilename());
    _wp += strlen(src->getFilename())+1;

    AA(_wp);    
    params[2].name = XPNR_HASH;
    params[2].type = XBT_SHA1;
    params[2].data = (_wp - _x);
// TODO: fill 
    memset(_wp,0xa1,20);    // sha1 -> a1 :-)
    _wp += 20; // allocating fixed 20 bytes

    params[3].name = XPNR_MODIFIED;
    params[3].type = XBT_UNIX_TIME;
    params[3].data = file_gettime(src->getFilename());  // direct stored
// children
    children[0] = WriteChildSymbols(&_wp,XTNR_TAG_SYMBOLS,SYMBOL_TABLE_NODES) - _x;
    children[1] = WriteChildSymbols(&_wp,XTNR_PARAM_SYMBOLS,SYMBOL_TABLE_PARAMS) - _x;
    children[2] = WritePayloadContainer(&_wp,XTNR_PAYLOAD,src->getRoot()) - _x;

//---------------------------
    X->length = _wp-data; // will set later
    return X->length;
}

const char *Bin_xml_creator::WriteChildSymbols(char **_wp,tag_name_id_t tag_name,int symbol_table)
{
    AA(*_wp); //===============================================================================

    XML_Item *X = reinterpret_cast<XML_Item *>(*_wp);
    const char *_x = *_wp;
    *_wp += sizeof(XML_Item);
    X->name = tag_name;
    X->paramcount = 2;  // count - how many symbols are stored here?
    X->childcount = 0;  // no children

    XML_Param_Description *params = reinterpret_cast<XML_Param_Description*>(*_wp);
    *_wp += sizeof(XML_Param_Description)*X->paramcount;

    int SC = this->symbol_count[symbol_table];
//-------------------------------------------------------------------------------------------------
    params[0].name = XPNR_COUNT;
    params[0].type = XBT_INT32;
    params[0].data = SC; // direct store
    
    params[1].name = XPNR_TYPE_COUNT;
    params[1].type = XBT_INT32;
    params[1].data = SC; // I will write default type of values

//-------------------------------------------------------------------------------------------------
    **_wp = XBT_BLOB;
    (*_wp)++;
    AA(*_wp);

// reference to place, where size of whole binary data is stored
    int32_t *size = reinterpret_cast<int32_t*>(*_wp);
    *_wp += sizeof(int32_t);


    const char *binary_block_begin = *_wp;  
    relative_ptr_t *actual_symbol_table = reinterpret_cast<relative_ptr_t *>(*_wp);
    *_wp += sizeof(relative_ptr_t)*SC;
    XML_Binary_Type_Stored *actual_symbol_types = reinterpret_cast<XML_Binary_Type_Stored*>(*_wp);
    *_wp += sizeof(XML_Binary_Type_Stored)*SC;
    

    for(int i = 0;i < SC;i++)
    {
        actual_symbol_table[i] = *_wp - _x; // tady to bude
        actual_symbol_types[i] = this->symbol_table_types[symbol_table][i];
        const char *src_name = this->symbol_table[symbol_table][i];
        strcpy(*_wp,src_name);
        *_wp += strlen(src_name)+1;
    }
    *size = *_wp - binary_block_begin;
    


//-------------------------------------------------------------------------------------------------
    AA(*_wp);
    X->length = *_wp - _x;
    DBG(std::cout << "SymbolCount[" << symbol_table << "] = " << SC << " TableSize = " << *size << " X->length = " << X->length << "\n");
    return _x;
}

struct XStoreParamsData {
    Bin_xml_creator     *creator;
    XML_Item        *X;
    XML_Param_Description   *params;
    relative_ptr_t      *children;
    char            **_wp;
    const char      *_x;
};

const char *Bin_xml_creator::WritePayloadContainer(char **_wp,tag_name_id_t tag_name,void *root)
{
    AA(*_wp); //===============================================================================

    XML_Item *X = reinterpret_cast<XML_Item *>(*_wp);
    const char *_x = *_wp;
    *_wp += sizeof(XML_Item);
    X->name = tag_name;
    X->paramcount = 0;  // count - how many symbols are stored here?
    X->childcount = 1;  // only one child

// array of children links
    relative_ptr_t *children = reinterpret_cast<relative_ptr_t *>(*_wp);
    *_wp += sizeof(relative_ptr_t)*X->childcount;
// data
    *((*_wp)++) = XBT_NULL; // no data

    children[0] = WriteNode(_wp,root) - _x;
    assert((children[0] & 3) == 0);

//-------------------------------------------------------------------------------------------------
    X->length = *_wp - _x;
    return _x;
}

const char *Bin_xml_creator::WriteNode(char **_wp,void *element)
{
    AA(*_wp); //===============================================================================

    XML_Item *X = reinterpret_cast<XML_Item *>(*_wp);
    const char *_x = *_wp;
    *_wp += sizeof(XML_Item);
    
//std::cout << /*(void*)(X)*/ (_x-data) << " " << element->name;

    X->name = Find(src->getNodeName(element),SYMBOL_TABLE_NODES);

    X->paramcount = 0;  // count - how many symbols are stored here?
    if (!src->ForAllBinParams(XCountBinParamsEvent,element,X))
        src->ForAllParams(XCountParamsEvent,element,X);
    
    X->childcount = 0;  // count them
    src->ForAllChildren(XCountChildrenEvent,element,X);
//-----------------------------------------------   
    XStoreParamsData xstore_data;
    xstore_data.creator = this;
    xstore_data.X = X;
    xstore_data.params = reinterpret_cast<XML_Param_Description*>(*_wp);
    xstore_data._wp = _wp;
    xstore_data._x = _x;
    *_wp += sizeof(XML_Param_Description)*X->paramcount;

    xstore_data.children = reinterpret_cast<relative_ptr_t*>(*_wp);
    *_wp += sizeof(relative_ptr_t)*X->childcount;

//-----------------------------------------------
    XML_Binary_Type type = XBT_UNKNOWN;
    int size = 0;
    const char *bin_value = src->getNodeBinValue(element,type,size);
    if (type != XBT_UNKNOWN) // getNodeBinValue supported?
    {
        if (bin_value == nullptr)
            *((*_wp)++) = XBT_NULL; // no value detected!
        else
        {
            *((*_wp)++) = type;
            ASSERT_NO_RET_NULL(1201,XBT_Copy(bin_value,type,size,_wp,data+data_size_allocated)); // copy of value
        }
    }
    else
    {
        const char *value = src->getNodeValue(element);
        if (value == nullptr)
            *((*_wp)++) = XBT_NULL; // no value detected!
        else
        {
            type = XBT_Detect(value);
            *((*_wp)++) = type;
            if (!XBT_FromString(value,type,_wp,data+data_size_allocated))
            {   
                LOG_ERROR("[A1202] XBT_FromString(%s,%s,...) failed",value,XML_BINARY_TYPE_NAMES[type]);
                return nullptr;
            }
        }
    }
//-----------------------------------------------
    if (!src->ForAllBinParams(XStoreBinParamsEvent,element,&xstore_data))
        src->ForAllParams(XStoreParamsEvent,element,&xstore_data);
    src->ForAllChildren(XStoreChildrenEvent,element,&xstore_data);

    X->length = *_wp - _x;
    assert(X->length > 0);
    return _x;
}

void Bin_xml_creator::XCountBinParamsEvent(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata)
{
    reinterpret_cast<XML_Item*>(userdata)->paramcount++; // how much they are
}

void Bin_xml_creator::XCountParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    reinterpret_cast<XML_Item*>(userdata)->paramcount++; // how much they are
}

void Bin_xml_creator::XCountChildrenEvent(void *element,void *userdata)
{
    reinterpret_cast<XML_Item*>(userdata)->childcount++; // how much they are
}

void Bin_xml_creator::XStoreBinParamsEvent(const char *param_name,int param_id,XML_Binary_Type type,const char *param_value,void *element,void *userdata)
{
    XStoreParamsData *xstore_data = reinterpret_cast<XStoreParamsData*>(userdata);
    // TODO: translate param_id
//    int name_id = 
        xstore_data->params->name = 
        xstore_data->creator->Find(param_name,SYMBOL_TABLE_PARAMS);

    if (param_value == nullptr || type == XBT_NULL)
    {
        xstore_data->params->type = XBT_NULL;
        xstore_data->params->data = 0;
        xstore_data->params++;
        return; // empty - it's fast
    }

    int content_size = 0;
    if (XBT_IS_VARSIZE(type))
    {
        AA(param_value);
        content_size = *reinterpret_cast<const int32_t*>(param_value);
    }
    else
    {
        int size = XBT_Size(type,0); // empty size of type
        char *in_place_wp = reinterpret_cast<char*>(&xstore_data->params->data);
        // bool XBT_Copy(const char *src,XML_Binary_Type type,int size,char **_wp,char *limit)
        
        if (type != XBT_STRING && size <= 4 && XBT_Copy(param_value,type,0/*size*/,&in_place_wp,xstore_data->creator->data+xstore_data->creator->data_size_allocated))
        { // store parameter value directly to params.data
            ASSERT_NO_DO_NOTHING(1207,in_place_wp == reinterpret_cast<char*>(&xstore_data->params->data) + 4);
            xstore_data->params->type = type;
            xstore_data->params++;
            return; // finished
        }
    }
    int align = XBT_Align(type);
    if (align == 4)
        AA(*xstore_data->_wp);
    else if (align == 8)
        AA8(*xstore_data->_wp);
         
    xstore_data->params->type = type;
    xstore_data->params->data = *xstore_data->_wp - xstore_data->_x;
/*    
    if (type == XBT_UNIX_TIME64_MSEC)
        LOG("-- Storing %s : time64 %s to pointer %p -> %p-%p ... offset=%d",
            param_name,XBT_ToString(type,param_value),param_value,*xstore_data->_wp,xstore_data->_x,xstore_data->params->data);
*/
    ASSERT_NO_DO_NOTHING(1206,XBT_Copy(param_value,type,content_size,xstore_data->_wp,xstore_data->creator->data+xstore_data->creator->data_size_allocated));
    xstore_data->params++;
}

void Bin_xml_creator::XStoreParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    XStoreParamsData *xstore_data = reinterpret_cast<XStoreParamsData*>(userdata);
    int name_id = 
        xstore_data->params->name = 
            xstore_data->creator->Find(param_name,SYMBOL_TABLE_PARAMS);

    if (param_value == nullptr || *param_value == '\0')
    {
        xstore_data->params->type = XBT_NULL;
        xstore_data->params->data = 0;
        xstore_data->params++;
        return; // empty - it's fast
    }

    XML_Binary_Type type = XBT_UNKNOWN;
    if (name_id >= 0)
        type = static_cast<XML_Binary_Type>(xstore_data->creator->symbol_table_types[SYMBOL_TABLE_PARAMS][name_id]);

    type = XBT_Detect2(param_value,type);
    int size = XBT_Size(type,0);
    char *in_place_wp = reinterpret_cast<char*>(&xstore_data->params->data);
    if (size == 4 && XBT_FromString(param_value,type,&in_place_wp,xstore_data->creator->data+xstore_data->creator->data_size_allocated))
    {
        xstore_data->params->type = type;
    }
    else
    {
        xstore_data->params->type = XBT_STRING;
        xstore_data->params->data = *xstore_data->_wp - xstore_data->_x;

        strcpy(*xstore_data->_wp,param_value);
        (*xstore_data->_wp) += strlen(param_value)+1;
    }

    xstore_data->params++;
}

void Bin_xml_creator::XStoreChildrenEvent(void *element,void *userdata)
{
    XStoreParamsData *xstore_data = reinterpret_cast<XStoreParamsData*>(userdata);
    *(xstore_data->children++) = xstore_data->creator->WriteNode(xstore_data->_wp,element) - xstore_data->_x;
}

void Bin_xml_creator::XStore2XBWEvent(void *element,void *userdata)
{
    MakingXBW_1node *node_info = reinterpret_cast<MakingXBW_1node*>(userdata);
    ASSERT_NO_RET(2055,node_info->src_element == element);
    Bin_xml_creator  *_this = node_info->creator;

    int tag_id = _this->Find(_this->src->getNodeName(element),SYMBOL_TABLE_NODES); // SLOW
    ASSERT_NO_RET(2056,tag_id >= 0);

    XML_Binary_Type tag_type;
    int value_size = 0;
    const char *value = _this->src->getNodeBinValue(element,tag_type,value_size);    // binary value referenced
    if (value == nullptr)
    {
        value = _this->src->getNodeValue(element);
        tag_type = static_cast<XML_Binary_Type>(_this->symbol_table_types[SYMBOL_TABLE_NODES][tag_id]);
    // THIS IS BAD: I have value in string but need size of this value as binary value
    // and this size can be obtained by decoding string. But decoding string is work and
    // if size of result is only result, it is waste of CPU time.
    // XBW is based on atomic memory allocation and expect multiple processes to allocate
    // at the same time. So size must be known at time of allocation.
    // There are 2 solutions:
    // 1. when single process - allocate more enough and return unused space later
    // 2. when multi process - measure size first and then allocate
    
    // NOW SLOW but SAFE solution
    // 1st pass - detecting real size
        int size = XBT_SizeFromString(value,tag_type); // TODO: not finished detection!!!
        
        
    }
    
    


//    BW_element *bw_root = W.tag(Find(src->getNodeName(root),SYMBOL_TABLE_NODES));
//    W.setRoot(bw_root);
//    src->ForAllChildren(,root,
//void Bin_json_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
    
}

//-------------------------------------------------------------------------------------------------

int Bin_xml_creator::FindOrAddTyped(const char *symbol,const int t,XML_Binary_Type type)
{
    if (symbol == nullptr) return -1;
    
// binary search
    int B = 0;
    int E = symbol_count[t]-1;
    while (B <= E)
    {
        int M = (B+E) >> 1;
        int cmp = strcmp(symbol,symbol_table[t][M]);
        if (cmp == 0) 
        {
            symbol_table_types[t][M] = static_cast<XML_Binary_Type_Stored>(
                XBT_JoinTypes(type,static_cast<XML_Binary_Type>(symbol_table_types[t][M])));
            return M;   // found
        }
        if (cmp < 0)
            E = M-1;
        else
            B = M+1;
    }
    if (symbol_count[t] >= MAX_SYMBOL_COUNT)
    {
        fprintf(stderr,"Cannot add symbol '%s' to symbol table of %s - limit %d reached",symbol,(t == SYMBOL_TABLE_NODES ? "tags" : "params"),MAX_SYMBOL_COUNT);
        return -1; // failed
    }
// not found --> add
    int P = E+1;
    assert(P >= 0 && P <= symbol_count[t]);
    if (symbol_count[t]-P > 0)
    {
        memmove(&symbol_table[t][P+1],&symbol_table[t][P],(symbol_count[t]-P)*sizeof(char*));
        memmove(&symbol_table_types[t][P+1],&symbol_table_types[t][P],(symbol_count[t]-P)*sizeof(XML_Binary_Type_Stored));
    }
    symbol_count[t]++;

    char *symbol_copy = reinterpret_cast<char *>(malloc(strlen(symbol)+1));
    strcpy(symbol_copy,symbol);
    symbol_table[t][P] = symbol_copy;
    symbol_table_types[t][P] = static_cast<XML_Binary_Type_Stored>(type);

#ifndef NDEBUG
// check ordered
    for(int i = 0;i < symbol_count[t]-1;i++)
        assert(strcmp(symbol_table[t][i],symbol_table[t][i+1]) < 0);
#endif
    return P; // position of insert
}

int Bin_xml_creator::FindOrAdd(const char *symbol,const int t,const char *value)
{
    if (symbol == nullptr) return -1;
    
// binary search
    int B = 0;
    int E = symbol_count[t]-1;
    while (B <= E)
    {
        int M = (B+E) >> 1;
        int cmp = strcmp(symbol,symbol_table[t][M]);
        if (cmp == 0) 
        {
            symbol_table_types[t][M] = static_cast<XML_Binary_Type_Stored>(

                XBT_Detect2(
                    value,
                    static_cast<XML_Binary_Type>(symbol_table_types[t][M])));
            return M;   // found
        }
        if (cmp < 0)
            E = M-1;
        else
            B = M+1;
    }
    if (symbol_count[t] >= MAX_SYMBOL_COUNT)
    {
        fprintf(stderr,"Cannot add symbol '%s' to symbol table of %s - limit %d reached",symbol,(t == SYMBOL_TABLE_NODES ? "tags" : "params"),MAX_SYMBOL_COUNT);
        return -1; // failed
    }
// not found --> add
    int P = E+1;
    assert(P >= 0 && P <= symbol_count[t]);
    if (symbol_count[t]-P > 0)
    {
        memmove(&symbol_table[t][P+1],&symbol_table[t][P],(symbol_count[t]-P)*sizeof(char*));
        memmove(&symbol_table_types[t][P+1],&symbol_table_types[t][P],(symbol_count[t]-P)*sizeof(XML_Binary_Type_Stored));
    }
    symbol_count[t]++;

    char *symbol_copy = reinterpret_cast<char *>(malloc(strlen(symbol)+1));
    strcpy(symbol_copy,symbol);
    symbol_table[t][P] = symbol_copy;
    symbol_table_types[t][P] = static_cast<XML_Binary_Type_Stored>(XBT_Detect(value));

#ifndef NDEBUG
// check ordered
    for(int i = 0;i < symbol_count[t]-1;i++)
        assert(strcmp(symbol_table[t][i],symbol_table[t][i+1]) < 0);
#endif
    return P; // position of insert
}


int Bin_xml_creator::Find(const char *symbol,const int t)
{
    if (symbol == nullptr) return -1;
    if (strcmp(symbol,"et-tecera") == 0) return XTNR_ET_TECERA;

// binary search
    int B = 0;
    int E = symbol_count[t]-1;
    while (B <= E)
    {
        int M = (B+E) >> 1;
        int cmp = strcmp(symbol,symbol_table[t][M]);
        if (cmp == 0) return M; // found
        if (cmp < 0)
            E = M-1;
        else
            B = M+1;
    }
    return -1; // not found
}



void Bin_xml_creator::ShowSymbols(const int t)
{
    std::cout << "[" << symbol_count[t] << "]: ";
    for(int i = 0;i < symbol_count[t];i++)
    {
        if (i > 0) std::cout << ", ";
        std::cout << symbol_table[t][i] << ":" << XML_BINARY_TYPE_NAMES[symbol_table_types[t][i]];
    }
}



const char *Bin_xml_creator::getNodeName(tag_name_id_t name_id)
{
    assert(name_id > XTNR_LAST && name_id < symbol_count[SYMBOL_TABLE_NODES]);
    if (name_id >= 0)
        return symbol_table[SYMBOL_TABLE_NODES][name_id];
    else switch(name_id)
    {
        case XTNR_NULL      : return "NULL";
        case XTNR_META_ROOT : return "META_ROOT";
        case XTNR_TAG_SYMBOLS   : return "TAG_SYMBOLS";
        case XTNR_PARAM_SYMBOLS : return "PARAM_SYMBOLS";
        case XTNR_HASH_INDEX    : return "HASH_INDEX";
        case XTNR_PAYLOAD   : return "PAYLOAD";
        case XTNR_REFERENCE : return "REFERENCE";
        case XTNR_PROCESSED : return "PROCESSED";
        case XTNR_ET_TECERA : return "ET_TECERA";
    }
    return "?";
}

const char *Bin_xml_creator::getNodeInfo(const XML_Item *X)
{
    assert(X != nullptr);
    const char *name = getNodeName(X->name);
    static char node_info[64];
    snprintf(node_info,sizeof(node_info),"#%d:%s",static_cast<relative_ptr_t>(reinterpret_cast<const char *>(X) - data),name);
    node_info[sizeof(node_info)-1] = '\0';
    return node_info;
}


//-------------------------------------------------------------------------------------------------
bool Bin_xml_creator::Convert(const char *src,const char *dst)
{
    Bin_xml_creator XC(src,dst);
    return XC.DoAll();
}


} // pklib_xml::
