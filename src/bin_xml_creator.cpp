#include "bin_xml_creator.h"
#include "bin_xml.h"
#include "files.h"
#include "ANSI.h"

#include <alloca.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h> // qsort_r
#include <stdio.h>

#include <iostream>

#define DBG(x)

namespace pklib_xml
{

//-------------------------------------------------------------------------------------------------

Bin_src_plugin::Bin_src_plugin(const char *filename,Bin_xml_creator *bin_xml_creator)
{
    /*
    // test1.xml --> test1.xb, test2 --> test2.xb, test3.doc --> test3.xb, test4.tar.gz --> test4.tar.xb
    const char *dot = strrchr(filename,'.');
    if (dot == nullptr) // not found? add .xb to filename
    {
    this->filename = new char[len+1+3];
    strcpy(this->filename,filename);
    strcpy(this->filename+len,".xb");
    }
    else
    {
    int len = dot - filename;
    this->filename = new char[len+1+3];
    strncpy(this->filename,filename,len);
    strcpy(this->filename+len,".xb");
    }
     */
    // I need original filename!!! - I cannot convert xml->xb without source filename!

    assert(sizeof(XML_Item) == 12);
    assert(sizeof(XML_Symbol_Info) == 8);
    assert(sizeof(XML_Param_Description) == 8);


    this->filename = nullptr;
    this->bin_xml_creator = bin_xml_creator;
    this->file_size = 0;
    
    setFilename(filename);
}

void Bin_src_plugin::setFilename(const char *filename)
{
    if (this->filename)
        delete [] this->filename;
    this->filename = nullptr;
    if (filename != nullptr)
    {
        int len = strlen(filename);
        this->filename = new char[len+1];
        strcpy(this->filename,filename);
    }
    else
        this->filename = nullptr;
}

Bin_src_plugin::~Bin_src_plugin()
{
    delete [] this->filename;
}

void Bin_src_plugin::LinkCreator(Bin_xml_creator *bin_xml_creator)
{
//    assert(this->bin_xml_creator == nullptr);
// it simple redirect creator to some other beast
    this->bin_xml_creator = bin_xml_creator;
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
        printf("%s",value);
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

//-------------------------------------------------------------------------------------------------

Bin_xml_creator::Bin_xml_creator(const char *src,const char *dst)
{
    assert(BIN_SRC_PLUGIN_SELECTOR != nullptr);

    this->src = BIN_SRC_PLUGIN_SELECTOR(src,this);
    this->src_allocated = true;
    this->dst = dst;
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

Bin_xml_creator::Bin_xml_creator(Bin_src_plugin *src,const char *dst)
{
    this->src = src;
    this->src->LinkCreator(this); // link it
    this->src_allocated = false;
    this->dst = dst;
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
    
}

//-------------------------------------------------------------------------------------------------

bool Bin_xml_creator::DoAll()
{
    if (!src->Initialize()) return false;

    // 1. filling symbol tables----------------------
    this->total_node_count = 4; // service elements!
    this->total_param_count = 0;
    this->total_value_count = 0;

    src->ForAllChildrenRecursively(FirstPassEvent,src->getRoot(),(void*)this,0);


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
    // 2. show stats---------------------------------

    DBG(std::cout << "total elements: " << total_node_count << "\n");
    DBG(std::cout << "total params: " << total_param_count << "\n");
    DBG(std::cout << "node names");
    DBG(ShowSymbols(SYMBOL_TABLE_NODES));
    DBG(std::cout << "\nparam names");
    DBG(ShowSymbols(SYMBOL_TABLE_PARAMS));
    DBG(std::cout << "\n");

    // 3. allocate and fill--------------------------
    this->data = reinterpret_cast<char *>(malloc(1024 + src->getFileSize()*2)); // some reserve

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
    int written = write(dst_file,data,dst_file_size);
    assert(written == dst_file_size);

    close(dst_file);


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
    char *wp = data;
    AA(wp);
    this->WriteNode(&wp,element);
    
    int size = wp - data;
    AA(size);
    int written = write(dst_file,data,size);
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
    _this->src->ForAllParams(FirstPassParamEvent,element,userdata);
    
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
    _this->src->ForAllParams(SecondPassParamEvent,element,userdata);
    
}

void Bin_xml_creator::SecondPassParamEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    Bin_xml_creator *_this = reinterpret_cast<Bin_xml_creator*>(userdata);
    _this->FindOrAdd(param_name,SYMBOL_TABLE_PARAMS,param_value);
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
    **_wp = XBT_BINARY;
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
    const char *value = src->getNodeValue(element);
    if (value != nullptr)
    {
        XML_Binary_Type type = XBT_Detect(value);
        if (type == XBT_INT32)
        {
            *((*_wp)++) = type;
            AA(*_wp);
//            std::cout << "Dbg: XBT_INT32 value: " << value << " ---> OFFSET: +" << ((*_wp) - _x) << "\n";
            *reinterpret_cast<int32_t*>(*_wp) = atoi(value);
            *_wp += sizeof(int32_t);
        }
        else
        {
            *((*_wp)++) = XBT_STRING; // it will be stored as string now
            strcpy(*_wp,value);
            //std::cout << " = " << value << "\n";
            *_wp += strlen(value)+1;
        }
    }
    else
    {
        *((*_wp)++) = XBT_NULL; // no value detected!
//std::cout << " ---\n";
    }
    
//-----------------------------------------------
    src->ForAllParams(XStoreParamsEvent,element,&xstore_data);
    src->ForAllChildren(XStoreChildrenEvent,element,&xstore_data);

    X->length = *_wp - _x;
    assert(X->length > 0);
    return _x;
}

void Bin_xml_creator::XCountParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    reinterpret_cast<XML_Item*>(userdata)->paramcount++; // how much they are
}

void Bin_xml_creator::XCountChildrenEvent(void *element,void *userdata)
{
    reinterpret_cast<XML_Item*>(userdata)->childcount++; // how much they are
}

void Bin_xml_creator::XStoreParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata)
{
    XStoreParamsData *xstore_data = reinterpret_cast<XStoreParamsData*>(userdata);

    xstore_data->params->name = xstore_data->creator->Find(param_name,SYMBOL_TABLE_PARAMS);
    if (param_value != nullptr && *param_value != '\0')
    {
        xstore_data->params->type = XBT_STRING;
        xstore_data->params->data = *xstore_data->_wp - xstore_data->_x;

        strcpy(*xstore_data->_wp,param_value);
        (*xstore_data->_wp) += strlen(param_value)+1;
    }
    else
    {
        xstore_data->params->type = XBT_NULL;
        xstore_data->params->data = 0;
    }
    xstore_data->params++;
}

void Bin_xml_creator::XStoreChildrenEvent(void *element,void *userdata)
{
    XStoreParamsData *xstore_data = reinterpret_cast<XStoreParamsData*>(userdata);
    *(xstore_data->children++) = xstore_data->creator->WriteNode(xstore_data->_wp,element) - xstore_data->_x;
}

//-------------------------------------------------------------------------------------------------

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
            symbol_table_types[t][M] = static_cast<XML_Binary_Type_Stored>(XBT_Detect2(
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
    std::cout << "[" << symbol_count[t] << "]";
    if (symbol_count[t] == 0) return;
    std::cout << " " << symbol_table[t][0];
    for(int i = 1;i < symbol_count[t];i++)
    {
        std::cout << "," << symbol_table[t][i];
        std::cout << ":" << XML_BINARY_TYPE_NAMES[symbol_table_types[t][i]];
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
