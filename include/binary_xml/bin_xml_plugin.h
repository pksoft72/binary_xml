#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "bin_xml_creator.h"

namespace pklib_xml {
class Bin_xml_plugin : public Bin_src_plugin // interface for xml input format
{
protected:
    xmlDoc      *doc;
    xmlNode     *root_element;
public:
    Bin_xml_plugin(const char *filename,Bin_xml_creator *bin_xml_creator);
    virtual ~Bin_xml_plugin();
    virtual bool Initialize();
    
    virtual void *getRoot();
    virtual const char *getNodeName(void *element);
    virtual const char *getNodeValue(void *element);

    virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata);
    virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);

    virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata);
};
} // pklib_xml::
