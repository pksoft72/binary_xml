#include "bin_xml.h"

#include "macros.h"
#include "files.h"

#include <iomanip>  // cout wide symbols ..
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h> // PATH_MAX
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "bin_xml_packer.h" // for autoconversion


namespace pklib_xml
{

const char *XML_BINARY_TYPE_NAMES[XBT_LAST] = {
    "NULL",
    "VARIABLE",
    "STRING",
    "BINARY",
    "INT32",
    "INT64",
    "FLOAT",
    "DOUBLE",
    "HEX",
    "GUID",
    "SHA1",
    "UNIX_TIME",
    "IPv4",
    "IPv6",
    "VARIANT"
};


bool XML_Item::Check(XB_reader *R,bool recursive) const
// I want to check everything to be 100% sure, data are ok
{
    if (this == nullptr) return false;  // NULL is not ok
    if (R == nullptr) return false;
    if (this < R->doc) return false; // out of space
    if (reinterpret_cast<const char *>(this)+sizeof(XML_Item) > reinterpret_cast<const char *>(R->doc)+R->size) return false;
    if (reinterpret_cast<const char *>(this)+length > reinterpret_cast<const char *>(R->doc)+R->size) return false;
    
    const char *p = reinterpret_cast<const char*>(this);
    const char *p0 = p;
    AA(p);
//    std::cout << "XML_Item::Check " << (void*)this << "\n";
    if (p != p0)
    {
        std::cout << ANSI_RED_BRIGHT "XML_Item " << (void*)this << " is not aligned!" ANSI_RESET_LF;
        return false;
    }
    
// name check
    if (name <= XTNR_LAST) return false;
    if (name >= R->tag_symbols.count) return false;
// size check
    if (sizeof(XML_Item)+paramcount*sizeof(XML_Param_Description)+childcount*sizeof(relative_ptr_t) > (unsigned)length) return false; // not safe to access params or children
// params check
    for(int p = 0;p < paramcount;p++)
    {
        const XML_Param_Description *PD = getParamByIndex(p);
        if (PD->name <= XPNR_LAST) return false; // too low
        if (PD->name >= R->param_symbols.count) return false; // too high
        if (PD->type < XBT_NULL) return false;
        if (PD->type >= XBT_LAST) return false;
    // inline parameters
        if (PD->type == XBT_NULL) continue;
        if (PD->type == XBT_INT32) continue;
        if (PD->type == XBT_FLOAT) continue;
        if (PD->type == XBT_UNIX_TIME) continue;
        if (PD->type == XBT_IPv4) continue; 
    // check link
        if (PD->data < 0) return false; // reference out of node is not allowed here and yet
        if (PD->data >= length) return false; // pointer out of limit
        

        switch (PD->type)
        {
            case XBT_STRING:
                {
                    int max_len = this->length - PD->data;
                    int len = strnlen(reinterpret_cast<const char *>(this)+PD->data,max_len);
                    return len < max_len;
                }
            case XBT_INT64:
                return PD->data+sizeof(int64_t) <= (unsigned)this->length;
            case XBT_UINT64:
                return PD->data+sizeof(uint64_t) <= (unsigned)this->length;
//            case XBT_UNIX_TIME:
//                return PD->data+sizeof(uint32_t) <= (unsigned)this->length;
            case XBT_UNIX_TIME64_MSEC:
                return PD->data+sizeof(uint64_t) <= (unsigned)this->length;
            case XBT_DOUBLE:
                return PD->data+sizeof(double) <= (unsigned)this->length;
            case XBT_SHA1:
                return PD->data+sizeof(hash192_t) <= (unsigned)this->length;
            case XBT_GUID:
                return PD->data+sizeof(GUID_t) <= (unsigned)this->length;
            case XBT_IPv6:
                return PD->data+sizeof(IPv6_t) <= (unsigned)this->length;
            default:
                std::cerr << "Error: Parameter type " << PD->type << " is not supported now!\n";
                return false;
        }
    }
// TODO: check value


    if (recursive)
        for(int i = 0;i < childcount;i++)
        {
            const XML_Item *X = getChildByIndex(i);
            if (X == nullptr) return false;
            if (!X->Check(R,true)) return false;
        }
            
    return true;
}

//-------------------------------------------------------------------------------------------------

int32_t s_int32_value = -1;
const int32_t *XML_Param_Description::getIntPtr(const XML_Item *X) const 
{
    if (this == nullptr) return nullptr;
    if (X == nullptr) return nullptr;
    if (type == XBT_INT32) return reinterpret_cast<const int32_t *>(&data);
    if (type == XBT_STRING) 
    {
        int value0 = atoi(reinterpret_cast<const char *>(X)+data);
        s_int32_value = value0;
        return &s_int32_value; // non reetrant!!!
    }
    return nullptr; 
}

char s_output[32] = "";
const char *XML_Param_Description::getString(const XML_Item *X) const
{ 
    if (this == nullptr) return nullptr;
    if (X == nullptr) return nullptr;
    if (type == XBT_STRING) return reinterpret_cast<const char *>(X)+data;
    if (type == XBT_INT32)
    {
        snprintf(s_output,sizeof(s_output)-1,"%d",(int)data);
        return s_output;
    }
    return nullptr;
}

//-------------------------------------------------------------------------------------------------

const XML_Param_Description *XML_Item::getParamByIndex(int i) const
{
    if (this == nullptr) return nullptr;
    if (i < 0) return nullptr;
    if (i >= paramcount) return nullptr;
    const XML_Param_Description *params = reinterpret_cast<const XML_Param_Description*>(this+1);
    return params+i;
}

const XML_Param_Description *XML_Item::getParamByName(XML_Param_Names code) const
{
    if (this == nullptr) return nullptr;
    if (paramcount == 0) return nullptr;
    const XML_Param_Description *params = reinterpret_cast<const XML_Param_Description*>(this+1);
    for(int p = 0;p < paramcount;p++)
        if (params[p].name == code) return params+p;
    return nullptr;
}

const relative_ptr_t *XML_Item::getChildren() const
{
    if (this == nullptr) return nullptr;
    if (childcount == 0) return nullptr;
    return reinterpret_cast<const relative_ptr_t*>(reinterpret_cast<const char*>(this+1)+paramcount*sizeof(XML_Param_Description));
}

const XML_Item *XML_Item::getChildByIndex(int i) const
{
    if (this == nullptr) return nullptr;
    if (i < 0 || i >= childcount) return nullptr;
    
    return reinterpret_cast<const XML_Item*>(reinterpret_cast<const char*>(this)+getChildren()[i]);
}

const XML_Item *XML_Item::getChildByName(XML_Tag_Names code) const
{
    if (this == nullptr) return nullptr;
    if (childcount == 0) return nullptr;
    const relative_ptr_t *children = reinterpret_cast<const relative_ptr_t*>(reinterpret_cast<const char*>(this+1)+paramcount*sizeof(XML_Param_Description));
    for(int ch = 0;ch < childcount;ch++)
    {
        const XML_Item *child = reinterpret_cast<const XML_Item*>(reinterpret_cast<const char*>(this)+children[ch]);
        if (child->name == code) return child;
    }
    return nullptr;
}

const XML_Item *XML_Item::getNextChild(XB_reader &R,int &i) const    // this method can show even extented xb elements
// this method can show even extented xb elements
{
    if (this == nullptr) return nullptr;
    
    if (i >= childcount) return nullptr; // natural end of file

    if (i >= 0)
    {
        const XML_Item *X = reinterpret_cast<const XML_Item*>(reinterpret_cast<const char*>(this)+getChildren()[i++]);
    
        if (X->name != XTNR_ET_TECERA) // OK, standard child
            return X;
    // this is ET_TECERA special tag and will be replaced with jump to next element
        i = -R.doc->length; // negative value means direct offset
    }
    
    int offset = -i;
    AA(offset);
    
    if (offset + sizeof(XML_Item) > R.size) return nullptr; // end of data -> no more can fit
    const XML_Item *X = reinterpret_cast<const XML_Item*>(reinterpret_cast<const char*>(R.doc) + offset);
    if (offset + X->length > R.size) return nullptr; // something is strange - no whole XML_Item is stored in file        
    // must find the first extended record
    offset += X->length;
    AA(offset); // align!
    
    i = -offset;
    return X;
}

void XML_Item::ForAllChildrenRecursively(OnItem_t on_item,void *userdata,int deep) const
{
    if (this == nullptr) return;
    ASSERT_NO_EXIT_1(1031,on_item != nullptr);
    for(int i = 0;i < childcount;i++)
    {
        const XML_Item *child = reinterpret_cast<const XML_Item*>(
                reinterpret_cast<const char*>(
                    this
                )+getChildren()[i]);

        on_item(child,userdata,deep);

        child->ForAllChildrenRecursively(on_item,userdata,deep+1);
    }
}

void  XML_Item::ForAllChildrenRecursively_PostOrder(OnItem_t on_item,void *userdata,int deep) const
{
    if (this == nullptr) return;
    ASSERT_NO_EXIT_1(1032,on_item != nullptr);
    for(int i = 0;i < childcount;i++)
    {
        const XML_Item *child = reinterpret_cast<const XML_Item*>(
                reinterpret_cast<const char*>(
                    this
                )+getChildren()[i]);

        child->ForAllChildrenRecursively_PostOrder(on_item,userdata,deep+1);
        on_item(child,userdata,deep);
    }
}

void  XML_Item::ForAllChildrenRecursively_PostOrderA(OnItemA_t on_item,void *userdata,int deep,const XML_Item **root) const
{
    if (this == nullptr) return;
    ASSERT_NO_EXIT_1(1175,on_item != nullptr);

    if (root != nullptr && deep < XML_MAX_DEEP)
        *(root+deep) = this;

    for(int i = 0;i < childcount;i++)
    {
        const XML_Item *child = reinterpret_cast<const XML_Item*>(
                reinterpret_cast<const char*>(
                    this
                )+getChildren()[i]);

        child->ForAllChildrenRecursively_PostOrderA(on_item,userdata,deep+1,root);
        on_item(child,userdata,deep,root);
    }
}

const char *XML_Item::getString() const
{
    if (this == nullptr) return nullptr;
    CHECK_AA_THIS;

    const char *p = reinterpret_cast<const char*>(this+1)
        +paramcount * sizeof(XML_Param_Description)
        +childcount * sizeof(relative_ptr_t);
    XML_Binary_Type t = static_cast<XML_Binary_Type>(*(p++));
    
    static char output[32];

    if (t == XBT_STRING) return p;
    AA(p);
    return XBT_ToString(t,p,output,sizeof(output));
}


const void *XML_Item::getBinary(int32_t *size) const
{
    if (this == nullptr) return nullptr;
    CHECK_AA_THIS;

    const char *p = reinterpret_cast<const char*>(this+1)
        +paramcount * sizeof(XML_Param_Description)
        +childcount * sizeof(relative_ptr_t);
    XML_Binary_Type t = static_cast<XML_Binary_Type>(*(p++));

    if (t == XBT_BINARY)
    {
        AA(p);
        if (size != nullptr)
            *size = *reinterpret_cast<const int32_t*>(p);
        p += sizeof(int32_t);
        return p;
    }
    else if (t == XBT_STRING)
    {
        if (size != nullptr)
            *size = strlen(p);
        return p;
    }
    return nullptr;
}

int32_t XML_Item::getInt() const
{
    if (this == nullptr) return INT32_NULL;
    CHECK_AA_THIS;

    const char *p = reinterpret_cast<const char*>(this+1)
        +paramcount * sizeof(XML_Param_Description)
        +childcount * sizeof(relative_ptr_t);
    XML_Binary_Type t = static_cast<XML_Binary_Type>(*(p++));

//    std::cout << "getInt(" << t << ")\n";
    
    if (t == XBT_INT32)
    {
        AA(p);
        return *reinterpret_cast<const int32_t*>(p);
    }
    else if (t == XBT_STRING)
    {
        if (p == nullptr) return INT32_NULL;
        return atoi(p);
    }
    return INT32_NULL;
}

const int32_t *XML_Item::getIntPtr() const
{
    if (this == nullptr) return nullptr;
    CHECK_AA_THIS;

    const char *p = reinterpret_cast<const char*>(this+1)
        +paramcount * sizeof(XML_Param_Description)
        +childcount * sizeof(relative_ptr_t);
    XML_Binary_Type t = static_cast<XML_Binary_Type>(*(p++));

//    std::cout << "getInt(" << t << ")\n";
    
    if (t == XBT_INT32)
    {
        AA(p);
        return reinterpret_cast<const int32_t*>(p);
    }
    else if (t == XBT_STRING)
    {
        if (p == nullptr) return nullptr;
        static int32_t p_result = atoi(p);
        return &p_result;
    }
    return nullptr;
}


const void XML_Item::write(std::ostream& os,XB_reader &R,int deep) const
{
    if (this == nullptr) return;
    if (R.verbosity > 0)
    {
        for(int t = 0;t < deep;t++) std::cerr << "\t";
        std::cerr << ANSI_BLACK_BRIGHT "#" << ((char*)this - (char*)R.doc) << "+" << this->length << " " << R.getNodeName(name) << "[" << paramcount << "][" << childcount << "]" ANSI_RESET_LF;
    }

#define TAB for(int t = 0;t < deep;t++) os << "\t"
    TAB;os << "<" << R.getNodeName(name);
    for(int p = 0;p < paramcount;p++)
    {
        const XML_Param_Description *param = getParamByIndex(p);
        os << " " << R.getParamName(param->name) << "=\"";
        const char *param_value = param->getString(this);
        if (param_value != nullptr) // TODO: escape param_value!
            os << param_value;
        os << "\"";
    }
    if (childcount > 0)
    {
        os << ">\n";
        int idx = 0;
        for(;;)
        {
            const XML_Item *child = this->getNextChild(R,idx);
            if (child == nullptr) break;
            child->write(os,R,deep+1);
            
        }
/*        const relative_ptr_t *children = getChildren();
        for(int ch = 0;ch < childcount;ch++)
        {
            const XML_Item *child = reinterpret_cast<const XML_Item*>(reinterpret_cast<const char*>(this)+children[ch]);
            child->write(os,R,deep+1);
        }*/
        const char *value = getString();
        if (value != nullptr && *value != 0)
        {
            TAB;os << value << "\n";    // TODO: escape value!
        }
        TAB;os << "</" << R.getNodeName(name) << ">\n";
    }
    else
    {
        const char *value = getString();
        if (value == nullptr || *value == '\0')
            os << "/>\n";
        else
            os << ">" << value << "</" << R.getNodeName(name) << ">\n";
    }
        
#undef TAB
}

//-------------------------------------------------------------------------------------------------

/*int32_t XML_Param_Description::getInt(const XML_Item *X) const
{
    if (this == nullptr) return INT32_NULL;
    if (X == nullptr) return INT32_NULL;
    if (type == XBT_INT32) return static_cast<int32_t>(data);
// TODO: all other types!
    return INT32_NULL; // reserved for null
}

const char *XML_Param_Description::getString(const XML_Item *X) const
{
    if (this == nullptr) return nullptr;
    if (X == nullptr) return nullptr;
    if (type == XBT_STRING) return reinterpret_cast<const char *>(X)+data;
    if (type == XBT_NULL) return "";
    return nullptr;
}*/

//-------------------------------------------------------------------------------------------------

_XB_symbol_table::_XB_symbol_table()
{
    count = 0;
// these pointers refers to xb content directly
    item_container = nullptr;
    reference_array = nullptr;
    type_array = nullptr;
}

_XB_symbol_table::~_XB_symbol_table()
{
    count = 0;
    item_container = nullptr;
    reference_array = nullptr;
    type_array = nullptr;
}

bool _XB_symbol_table::Load(const XML_Item *container)
// this will load=link symbol table to data in .xb
// symbol table is in <tag_symbols count="999">link[0],link[1],...,link[count-1]|linked strings</tag_symbols>
// or in <param_symbols ...> element
{
    item_container = container;
    count = container->getParamByName(XPNR_COUNT)->getInt(container);
    if (count == INT32_NULL) return false;
    if (count == 0) return true; // ok, no symbols

    type_count = container->getParamByName(XPNR_TYPE_COUNT)->getInt(container);
    if (type_count == INT32_NULL)
        type_count = 0;

    
    int32_t size = -1; // size of whole data block - reference_array & strings
    reference_array = reinterpret_cast<const relative_ptr_t*>(container->getBinary(&size));
/*  if (size != count * sizeof(relative_ptr_t))
    {
        std::cerr << "Error reading symbol table of " << count << " elements with size " << size << " bytes (should be " << count * sizeof(relative_ptr_t) << ")\n";
        return false;
    }*/
//    std::cout << "*** Reading symbol table of " << count << " elements with size " << size << " bytes.\n";
    if (reference_array == nullptr) return false;   // count != 0
    
    type_array = reinterpret_cast<const XML_Binary_Type_Stored*>(reference_array+count);
    const char *A0 = reinterpret_cast<const char*>(type_array+type_count);  // start of string area after last item in reference_array
    const char *A1 = reinterpret_cast<const char*>(reference_array)+size;   // end of string area as begin of binary data + it's size
    if (A0 > A1) return false; // no place for strings and reference_arrays
    for(int s = 0;s < count;s++)
    {
        const char *value = reinterpret_cast<const char*>(container) + reference_array[s];
        if (value < A0) return false;
        if (value >= A1) return false;
        int max_len = A1-value;
        int len = strnlen(value,max_len);
        if (len == max_len) return false; // unterminated string
    }
    return true;
}

const char *_XB_symbol_table::getSymbol(tag_name_id_t name_id) const
{
    if (name_id < 0) return XTNR2str(name_id);
    if (name_id >= count) return "__out_of_range__";
    return reinterpret_cast<const char *>(item_container)+reference_array[name_id];
}


int _XB_symbol_table::getID(const char *name) const
{
    int B = 0;
    int E = count - 1;
// interval B <= X <= E
    while (B <= E)
    {
        int M = (B+E) >> 1;
        int cmp = strcmp(name,reinterpret_cast<const char*>(item_container)+reference_array[M]);
        if (cmp < 0)
            E = M-1;
        else if (cmp > 0)
            B = M+1;
        else
            return M;
    }
    return -1; // NOT FOUND == XTNR_NULL == XPNR_NULL
}

//-------------------------------------------------------------------------------------------------

XB_reader::XB_reader(char *filename)
{
    this->filename = filename;
    this->size = -1;
    this->fd = -1;
    
    this->doc = reinterpret_cast<XML_Item*>(MAP_FAILED);
    this->data = nullptr;
}

XB_reader::~XB_reader()
{
//    if (!Finalize()) CANNOT CALL VIRTUAL METHODS IN DESTRUCTOR!!!
//        std::cerr << "Error in XB_reader(" << this->filename << ")::Finalize\n";
#ifndef NDEBUG
//    printf("~XB_reader(%s)\n",this->filename);
#endif
}

bool XB_reader::setFilename(char *filename)  // can set filename later
{
    ASSERT_NO_RET_FALSE(1176,this->filename == nullptr);
    this->filename = filename;
    return true;
}

void XB_reader::setVerbosity(int verbosity)
{
    this->verbosity = verbosity;
}

bool XB_reader::Initialize()
{
//    printf("XB_reader::Initialize\n");
//    printf("filename=%s\n",this->filename);

// conversion
    char *last_dot = strrchr(this->filename,'.');
    if (last_dot != nullptr && strcmp(last_dot,".xml") == 0) // it is xml!
    {
        char xml_filename[PATH_MAX];
        STRCPY(xml_filename,this->filename);
        *(last_dot++) = '.';
        *(last_dot++) = 'x';
        *(last_dot++) = 'b';
        *(last_dot++) = '\0';
        
        printf("convert xml to %s\n",this->filename);
        if (!pklib_xml::Bin_xml_packer::Convert(xml_filename,this->filename))
            return false;

    }
    else if (last_dot != nullptr && strcmp(last_dot,".json") == 0) // it is xml!
    {
        char json_filename[PATH_MAX];
        STRCPY(json_filename,this->filename);
        *(last_dot++) = '.';
        *(last_dot++) = 'x';
        *(last_dot++) = 'b';
        *(last_dot++) = '\0';
        
        if (!pklib_xml::Bin_xml_packer::Convert(json_filename,this->filename))
            return false;
    }

// open
    fd = open(this->filename,O_RDONLY,0);
    if (fd == -1)
    {
        ERRNO_SHOW(1024,"open",this->filename);
        return false;
    }

    size = file_getsize(this->filename);
    assert(size > sizeof(XML_Item));

    doc = reinterpret_cast<XML_Item*>(mmap(NULL,size,PROT_READ,MAP_SHARED,fd,0));
    if (doc == MAP_FAILED)
    {
        ERRNO_SHOW(1025,"mmap",filename);
        return false;
    }
    assert(doc->name == XTNR_META_ROOT);

    // check only root element
    if (!doc->Check(this,false)) return false;

    const XML_Item *X = doc->getChildByName(XTNR_TAG_SYMBOLS);
    if (!X->Check(this,false)) return false;
    if (!tag_symbols.Load(X)) return false;

    X = doc->getChildByName(XTNR_PARAM_SYMBOLS);
    if (!X->Check(this,false)) return false;
    if (!param_symbols.Load(X)) return false;


    // recursive check all elements - both symbol tables are loaded -> can check recursively
    if (!doc->Check(this,true)) return false;   

    //    std::cout << "DBG: data = " << (void*)doc;

    data = doc->getChildByName(XTNR_PAYLOAD)->getChildByIndex(0);

    return true;
}

bool XB_reader::Finalize()
{
#ifndef NDEBUG
//    printf("XB_reader::Finalize(%s)\n",this->filename);
#endif
    if (doc != MAP_FAILED)
    {
        if (munmap((void*)doc,size) == -1)
        {
            ERRNO_SHOW(1026,"munmap",this->filename);
            return false;
        }
        doc = reinterpret_cast<XML_Item*>(MAP_FAILED);
        data = nullptr;
    }
    if (fd != -1)
    {
        if (close(fd) == -1)
        {
            ERRNO_SHOW(1027,"close",this->filename);
            return false;
        }
        fd = -1;
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, XB_reader& R)
{
//  os << "<!-- Content of file " << R.filename << ": -->\n";       
    
    R.doc->getChildByName(XTNR_PAYLOAD)->getChildByIndex(0)->write(os,R,0);

    return os;
}

std::ostream& operator<<(std::ostream& os, const hash192_t& h)
{
    os << std::hex;
    for(int x = 0;x < 20;x++)
        os << std::setfill('0') << std::setw(2) << ((unsigned)h[x] & 0xff);
    os << std::dec;
    return os;
}

const char *XTNR2str(int16_t name)
{
    switch(name)
    {
        case XTNR_NULL: return "NULL";
        case XTNR_META_ROOT: return "meta_root";
        case XTNR_TAG_SYMBOLS: return "tag_symbols";
        case XTNR_PARAM_SYMBOLS: return "param_symbols";
        case XTNR_HASH_INDEX: return "hash_index";
        case XTNR_PAYLOAD: return "payload";
        case XTNR_REFERENCE: return "reference";
        case XTNR_PROCESSED: return "processed";
        case XTNR_ET_TECERA: return "et-tecera";
        default:            return "__reserved__";
    }
}

} // pklib_xml::
