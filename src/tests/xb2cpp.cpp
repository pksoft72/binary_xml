#include "bin_xml_generator.h"
#include <assert.h>

int main(int argc,char **argv)
{
	for(int i = 1;i < argc;i++)
	{
		pklib_xml::XB_generator G(argv[i]);
		assert(G.Initialize());
		std::cout << G;
		assert(G.Finalize());
	}
}
