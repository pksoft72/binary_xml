#include "bin_xml.h"

#include <iostream>
#include <limits.h>
#include <string.h>
#include <assert.h>

int main(int argc,char **argv)
{
    int verbosity = 0;
    for(int i = 1;i < argc;i++)
    {
        if (strcmp(argv[i],"-v") == 0)
            verbosity++;
        else
        {        
            pklib_xml::XB_reader R(argv[i]);
            R.setVerbosity(verbosity);
            assert(R.Initialize());

            std::cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            std::cout << R;

            assert(R.Finalize());
        }
    }
    return 0; // ok
}
