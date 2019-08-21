#include <assert.h>

#include "bin_xml.h"
#include "bin_xml_creator.h"
#include "bin_xml_packer.h"
#include "bin_write_plugin.h"

using namespace pklib_xml;

// This is only testing - not stable coding style of new xml creation from scratch.
// Normally I need tree representation of all objects and several passes of that structure.
// But each object can provide it's properties in some simple way.

#define TAG_MAIN        0
#define TAG_PERSON      1
#define TAG_NAME        2
#define TAG_SURNAME     3

#define ATTR_ID         0

void MakeDictionary(BW_plugin &W)
{
    W.registerElement(TAG_MAIN,"main",XBT_NULL);
    W.registerElement(TAG_PERSON,"person",XBT_NULL);
    W.registerElement(TAG_NAME,"name",XBT_STRING);
    W.registerElement(TAG_SURNAME,"surname",XBT_STRING);

    W.registerAttribute(ATTR_ID,"ID",XBT_INT32);
    assert(W.Initialize()); // prepare symbol tables
}


int main(int argc,char **argv)
{
    // Bin_xml_creator is standard way of converting xml or json to xb
    {
        printf("creating test0.xb ... ");
        BW_plugin W(0x1000,nullptr);
        MakeDictionary(W);
        W.setRoot(W.tag(TAG_MAIN)); // <main></main>
        assert(Bin_xml_packer::Convert(&W,"test0.xb"));
        printf("%d B\n",(int)file_getsize("test0.xb"));
    }   
    {
        printf("creating test1.xb ... ");
        BW_plugin W(0x1000,nullptr);
        MakeDictionary(W);
        W.setRoot(                  // <main ID="1"></main>
                W.tag(TAG_MAIN)
                .attrInt32(ATTR_ID,1)
                );
        assert(Bin_xml_packer::Convert(&W,"test1.xb"));
        printf("%d B\n",(int)file_getsize("test1.xb"));
    }
    {
        printf("creating test1.xb ... ");
        BW_plugin W(0x1000,nullptr);
        MakeDictionary(W);
        W.setRoot(W.tag(TAG_MAIN)             // <main><person><name>Petr</name><surname>Kundrata</surname></person></main>
                .add(W.tag(TAG_PERSON).attrInt32(ATTR_ID,1)
                    .add(W.tagStr(TAG_NAME,"Petr"))
                    .add(W.tagStr(TAG_SURNAME,"Kundrata"))
                    ));
        assert(Bin_xml_packer::Convert(&W,"test2.xb"));
        printf("%d B\n",(int)file_getsize("test2.xb"));
    }
    {
        printf("creating test3.xb ... ");
        BW_plugin W(0x1000,nullptr);
        MakeDictionary(W);
        W.setRoot(W.tag(TAG_MAIN)             // <main><person><name>Petr</name><surname>Kundrata</surname></person></main>
                .add(W.tag(TAG_PERSON).attrInt32(ATTR_ID,1)
                    .add(W.tagStr(TAG_NAME,"Petr"))
                    .add(W.tagStr(TAG_SURNAME,"Kundrata"))
                    )
                .add(W.tag(XTNR_ET_TECERA))
        );
        assert(Bin_xml_packer::Convert(&W,"test3.xb"));
        printf("%d B\n",(int)file_getsize("test3.xb"));
    }




}
