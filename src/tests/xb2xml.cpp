#include "bin_xml.h"

#include <iostream>
#include <limits.h>
#include <string.h>
#include <assert.h>

int main(int argc,char **argv)
{
    for(int i = 1;i < argc;i++)
    {
        pklib_xml::XB_reader R(argv[i]);
        assert(R.Initialize());
        
        std::cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        std::cout << R;
        
        assert(R.Finalize());
    }
    return 0; // ok
}
