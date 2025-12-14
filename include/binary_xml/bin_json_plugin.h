#pragma once

#include "bin_xml_creator.h"
#include "jsmn/jsmn.h"

namespace pklib_xml {

class Bin_json_plugin : public Bin_src_plugin // interface for json input format
{
protected:
	int		fd;
	const char	*source;
	int		num_tokens0;
	int		num_tokens1;
	int		max_str_len;
	char		*last_str;
	jsmntok_t 	*tokens;
public:
	Bin_json_plugin(const char *filename,Bin_xml_creator *bin_xml_creator);
	virtual ~Bin_json_plugin();
	virtual bool Initialize();
	
	virtual void *getRoot();
	virtual const char *getNodeName(void *element);
	virtual const char *getNodeValue(void *element);
	virtual void ForAllChildren(OnElement_t on_element,void *parent,void *userdata);
	virtual void ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);
	virtual void ForAllParams(OnParam_t on_param,void *element,void *userdata);
};

}	// pklib_xml::
