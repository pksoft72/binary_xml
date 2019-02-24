#pragma once
#include "bin_xml_creator.h"

namespace pklib_xml {

class Bin_xml_packer : public Bin_xml_creator
{
	char *data2;		// i need this to compute relative addresses
protected:
	virtual int32_t	Pack();
private:
		void MarkReferences();
		char *PackNode(char **_wp,char *X);
public: //---------------------------------------
	Bin_xml_packer(const char *src,const char *dst);
    Bin_xml_packer(Bin_src_plugin *src,const char *dst);
	static bool Convert(const char *src,const char *dst);
	static bool Convert(Bin_src_plugin *src,const char *dst);
    static bool GetXB(const char *xml_filename,char *xb_filename,const char **err_message);
};

}


