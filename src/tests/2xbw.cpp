#include "bin_xml.h"
#include "bin_xml_creator.h"
#include "bin_xml_packer.h"
#include "bin_xml_types.h"

#include <stdio.h>
#include <string.h> // strcpy...
#include <limits.h> // PATH_MAX
#include <cstddef>  // nullptr

int main(int argc,char **argv)
{
    printf("2xbw\n");

    
    for(int i = 1;i < argc;i++)
    {
        if (strcmp(argv[i],"--test") == 0)
            return (pklib_xml::XBT_Test() ? 0 : 1);

        char dst[PATH_MAX];
        strcpy(dst,argv[i]);
        char *dot = strrchr(dst,'.');
        if (dot == NULL) return 1;
        if (dot-dst+5 > PATH_MAX) return 1;
        dot++;
        *(dot++) = 'x';
        *(dot++) = 'b';
        *(dot++) = 'w';
        *(dot++) = '\0';

        printf("Input:  %s\n",argv[i]);
        printf("Output: %s\n",dst);
        pklib_xml::Bin_xml_creator XC(argv[1],dst,pklib_xml::XBTARGET_XBW);
        if (!XC.DoAll())
            return 1;
    }
    
    return 0; // ok
}

