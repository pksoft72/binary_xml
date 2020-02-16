#pragma once

#include "files.h"
#include "bin_xml.h"

namespace pklib_xml {

enum SymbolTableTypes
{
    SYMBOL_TABLE_NODES,
    SYMBOL_TABLE_PARAMS,
    SYMBOL_TABLES_COUNT
};

const int MAX_SYMBOL_COUNT = 32768; // symbols are stored into 16bit words, negative values are reserved
const int TAB_SIZE = 4;

class Bin_xml_creator;

typedef void (*OnElement_t)(void *element,void *userdata);
typedef void (*OnElementRec_t)(void *element,void *userdata,int deep);
typedef void (*OnParam_t)(const char *param_name,const char *param_value,void *element,void *userdata);

class Bin_src_plugin // interface for universal tree input format
{
protected:
    char        *filename;
    off_t       file_size;
    Bin_xml_creator *bin_xml_creator;   // as userdata for all methods
public:
    Bin_src_plugin(const char *filename,Bin_xml_creator *bin_xml_creator);
    virtual ~Bin_src_plugin();
    virtual bool Initialize();
    void LinkCreator(Bin_xml_creator *bin_xml_creator);

    virtual void *getRoot()=0;
    virtual const char *getNodeName(void *element)=0;
    virtual const char *getNodeValue(void *element)=0;
    virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata)=0;
    virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep)=0;
    virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata)=0;

    off_t   getFileSize() {return file_size;}
    const char *getFilename() {return filename;}

    void print();
private:
    int print_deep;
    int print_node_counter;
    static void s_PrintElement(void *element,void *userdata);
    static void s_PrintParam(const char *param_name,const char *param_value,void *element,void *userdata);
};

typedef Bin_src_plugin* (*Bin_src_plugin_selector_t)(const char *filename,Bin_xml_creator *bin_xml_creator);

class Bin_xml_creator
{
public:
    Bin_src_plugin  *src;
protected:
    bool        src_allocated;
    const char  *dst;

    int32_t     dst_file_size;
    char        *data;

// 1st pass to enumerate paramcount
    int     total_node_count;
    int     total_param_count;
    int     total_value_count;
// 2nd pass to fill tables
    const char              **symbol_table[SYMBOL_TABLES_COUNT];
    XML_Binary_Type_Stored  *symbol_table_types[SYMBOL_TABLES_COUNT];
    int                     symbol_count[SYMBOL_TABLES_COUNT];
// incremental addition
    int     dst_file;
public:
    Bin_xml_creator(const char *src,const char *dst);
    Bin_xml_creator(Bin_src_plugin *src,const char *dst);
    virtual ~Bin_xml_creator();

    bool DoAll();
//        bool PrepareDestination();  
    bool Append(void *element);
protected:
        static void FirstPassEvent(void *element,void *userdata,int deep);
        static void FirstPassParamEvent(const char *param_name,const char *param_value,void *element,void *userdata);
        static void SecondPassEvent(void *element,void *userdata,int deep);
        static void SecondPassParamEvent(const char *param_name,const char *param_value,void *element,void *userdata);
        
        int32_t FillData();
            const char *WriteChildSymbols(char **_wp,tag_name_id_t tag_name,int symbol_table);
            const char *WritePayloadContainer(char **_wp,tag_name_id_t tag_name,void *root);
                static void XCountParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata);
                static void XCountChildrenEvent(void *element,void *userdata);
                static void XStoreParamsEvent(const char *param_name,const char *param_value,void *element,void *userdata);
                static void XStoreChildrenEvent(void *element,void *userdata);
            const char *WriteNode(char **_wp,void *element);

        virtual int32_t Pack();

    int FindOrAdd(const char *symbol,const int t,const char *value);
    int Find(const char *symbol,const int t);
    void ShowSymbols(const int t);
    const char *getNodeName(tag_name_id_t name_id);
    const char *getNodeInfo(const XML_Item *X);
public: //---------------------------------------
    static bool Convert(const char *src,const char *dst);
};

extern const Bin_src_plugin_selector_t BIN_SRC_PLUGIN_SELECTOR;

}   // pklib_xml::
