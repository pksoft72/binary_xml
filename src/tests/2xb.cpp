#include <binary_xml/bin_xml.h>
#include <binary_xml/bin_xml_creator.h>
#include <binary_xml/bin_xml_packer.h>
#include <binary_xml/bin_xml_types.h>
#include <binary_xml/macros.h>

#include <stdio.h>
#include <string.h> // strcpy...
#include <limits.h> // PATH_MAX
#include <cstddef>  // nullptr

int main(int argc,char **argv)
{
    printf("2xb\n");

    
    for(int i = 1;i < argc;i++)
    {
        if (strcmp(argv[i],"--test") == 0)
            return (pklib_xml::XBT_Test() ? 0 : 1);

        // extension change is done in constructor
        pklib_xml::Bin_xml_creator XC(argv[1],argv[1],pklib_xml::XBTARGET_XB);
        if (!XC.Make_xb())
            return 1;
    }
    
    return 0; // ok
}
