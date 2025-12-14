#include <iostream>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include <binary_xml/files.h>
#include <binary_xml/macros.h>

#include <binary_xml/bin_xml.h>
#include <binary_xml/bin_write_plugin.h>

int main(int argc,char **argv)
{
    int verbosity = 0;
    for(int i = 1;i < argc;i++)
    {
        if (strcmp(argv[i],"-v") == 0)
            verbosity++;
        else
        {
            char *dot = strrchr(argv[i],'.');

            if (dot != nullptr && strcmp(dot,".xbw") == 0)
            {
                off_t file_size = (int)file_getsize(argv[i]);
                ASSERT_NO_RET_1(1208,file_size > 0);
                int max_pool_size = (((file_size * 3) >> 1) + 0x4000) >> 12 << 12;
                
                // BW_plugin(const char *filename,Bin_xml_creator *bin_xml_creator,int max_pool_size);
                pklib_xml::BW_plugin BW(argv[i],nullptr,max_pool_size,0);
                ASSERT_NO_RET_1(1209,BW.Initialize());

                std::cout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
                std::cout << "<!--\nTAG NAMES:\n";
                pklib_xml::BW_pool *pool = BW.getPool();
                for(int i = 0;i <= pool->tags.max_id;i++)
                    std::cout << "\t" << pool->getTagName(i) << " : " << XBT2STR(pool->getTagType(i)) << "\n";

                std::cout << "\nATTR NAMES:\n";
                for(int i = 0;i <= pool->params.max_id;i++)
                    std::cout << "\t" << pool->getAttrName(i) << " : " << XBT2STR(pool->getAttrType(i)) << "\n";
                std::cout << "-->\n";
                std::cout << BW;

//                ASSERT_NO_RET_1(1210,BW.Finalize());
            }
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
    }
    return 0; // ok
}
