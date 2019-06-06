#include "bin_xml.h"
#include "bin_xml_creator.h"
#include "bin_write_plugin.h"

// This is only testing - not stable coding style of new xml creation from scratch.
// Normally I need tree representation of all objects and several passes of that structure.
// But each object can provide it's properties in some simple way.

#define TAG_MAIN        0
#define TAG_PERSON      1
#define TAG_NAME        2
#define TAG_SURNAME     3

#define ATTR_ID         0


int main(int argc,char **argv)
{
// Bin_xml_creator is standard way of converting xml or json to xb
    Bin_write_plugin W(0x1000,nullptr);

    W.registerElement(TAG_MAIN,"main",XBT_NULL);
    W.registerElement(TAG_PERSON,"person",XBT_NULL);
    W.registerElement(TAG_NAME,"name",XBT_STRING);
    W.registerElement(TAG_SURNAME,"surname",XBT_STRING);

    W.registerAttribute(ATTR_ID,"ID",XBT_INT32);

    assert(W.Initialize()); // prepare symbol tables

// I neet some nice and error-prone recursive representation

    W.setRoot(W.tag(TAG_MAIN)

    W.setRoot(
        W.tag(TAG_MAIN)
            .attrInt(ATTR_ID,1)
            .children(
            
            )
    );

    W.tag(TAG_MAIN).children(
        W.tag(TAG_PERSON).attrInt32(ATTR_ID,1).children(
            W.tag(TAG_NAME).valueString("Petr").
            W.tag(TAG_SURNAME).valueString("Kundrata")
        )
    );
    W.set( // will set root
        W.tag(TAG_MAIN).children(
            W.tagA(TAG_PERSON,W.attrInt32(ATTR_ID,1),nullptr).children(
                W.tag(TAG_NAME).value("Petr"),
                W.tag(TAG_SURNAME).value("Kundrata"),
                nullptr
            )
        )
    );
    Bin_written_element root = W.addTag(


    W.set(
        
    );

    assert(Bin_xml_packer.Convert(W,"test.xb"));
}
