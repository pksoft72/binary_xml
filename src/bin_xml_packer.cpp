#include "bin_xml_packer.h"
#include <assert.h>
#include <string.h>

#include <iostream>
#include <iomanip>  // cout wide symbols ..
//#include <openssl/sha.h>
#include "missings.h"   // sha.h ...

#include "bin_xml_plugin.h"
#include "bin_json_plugin.h"
#include "bin_write_plugin.h"
#include "ANSI.h"

#define DBG(x) x
#define DBG2(x) x

namespace pklib_xml {

// private structures

class _XML_Reference // private dummy content to replace XML_node
{
public:
    tag_name_id_t   name;       // = XTNR_REFERENCE/XTNR_PROCESSED
    int16_t     padding;
    relative_ptr_t  reference;  // relative pointer to previos copy
    int32_t     length;     // compatible with XML_Item
};

struct _SortingNodes
{
    int             total_node_count;
    relative_ptr_t *node_links;
    hash192_t      *node_hashes;
    int            *index;
    
    static int compare(const void *_A,const void *_B,void *_SN)
    {
        _SortingNodes *SN = reinterpret_cast<_SortingNodes*>(_SN);
        int A = *reinterpret_cast<const int*>(_A);
        int B = *reinterpret_cast<const int*>(_B);
        assert(A >= 0 && A < SN->total_node_count);
        assert(B >= 0 && B < SN->total_node_count);
    // primary by hash
        int R = memcmp(SN->node_hashes[A],SN->node_hashes[B],sizeof(hash192_t));
        if (R != 0) return R;
    // secondary by node_links
        if (SN->node_links[A] < SN->node_links[B]) return -1;
        else if (SN->node_links[A] > SN->node_links[B]) return 1;
        else return 0;
    }
};

//-------------------------------------------------------------------------------------------------

Bin_src_plugin *Bin_src_plugin_selector(const char *filename,Bin_xml_creator *bin_xml_creator)
// will create input plugin by analysing filename extension
{
    const char *dot = strrchr(filename,'.');
    if (dot == nullptr) return nullptr; // no extension
    if (strcmp(dot,".xml") == 0)
        return new Bin_xml_plugin(filename,bin_xml_creator);
    if (strcmp(dot,".json") == 0)
        return new Bin_json_plugin(filename,bin_xml_creator);
    if (strncmp(filename,"<internal-write>",16) == 0)
        return new BW_plugin(atoi(dot+1),bin_xml_creator);
    return nullptr;
}

const Bin_src_plugin_selector_t BIN_SRC_PLUGIN_SELECTOR = Bin_src_plugin_selector;


//-------------------------------------------------------------------------------------------------

int32_t Bin_xml_packer::Pack()
{
    MarkReferences();

    DBG(std::cout << "\n===========================================================\n");
// now i should recursively copy/repair all
    data2 = new char [dst_file_size]; // reinterpret_cast<char *>(alloca(dst_file_size));
    char *_wp = data2;

    XML_Item *X_dst = reinterpret_cast<XML_Item*>(PackNode(&_wp,this->data));
    memcpy(data,data2,X_dst->length);
    int32_t size = X_dst->length;
    
    delete [] data2;data2 = nullptr;
    return size;
}

void Bin_xml_packer::MarkReferences()
// 0. After construction - nodes are hierarchival ordered and have no references outside of node
// 1. Nodes will get hashes computed (thanks to 0.)
// 2. It will enable easily to compare 2 nodes
// 3. Only the 1st node (with the same hash) in file will survive, others will be removed and replaced by reference to the 1st (marked as XTNR_REFERENCE)
{
    DBG(std::cout << "Computing hashes ...";)   

// Prepare tables SN.node_links[0..total_node_count-1],SN.node_hashes[0..total_node_count-1]
    _SortingNodes SN;

    SN.total_node_count = total_node_count;

    //size_t size;
    SN.node_links = new relative_ptr_t[total_node_count];// reinterpret_cast<relative_ptr_t*>(alloca(size = sizeof(relative_ptr_t)*total_node_count));
    memset(SN.node_links,0,sizeof(relative_ptr_t)*total_node_count);
    SN.node_hashes = new hash192_t[total_node_count]; // reinterpret_cast<hash192_t*>(alloca(size = sizeof(hash192_t)*total_node_count));
    memset(SN.node_hashes,0,sizeof(hash192_t)*total_node_count);
    int filled_links = 0;
    int computed_hashes = 0;
    SN.node_links[filled_links++] = 0;  // reference to root
// each node is added to node_links (filled_links incremented) and later
// is processed (computed_hashes incremented).
// Processing contains hash computing and adding all children to node_links.
// flat loop :-) each tree node will add it's children to node_links[] array -> all nodes will be processed
    DBG2(std::cout << "Filling node_links[] and node_hashes[] array...\n");;
    DBG2(std::cout << "#<idx> actual: <relative_id>\n");
    DBG(std::cout << "#(<idx>/<all>): [<childidx>/<all_children>] = ----> (<child global index>/<all_nodes>)\n");
    while (computed_hashes < filled_links) // this is "ok - done" condition
    {
        assert(computed_hashes <= total_node_count);
        assert(filled_links <= total_node_count);

        relative_ptr_t my_rel = SN.node_links[computed_hashes];
        XML_Item *X = reinterpret_cast<XML_Item*>(data+my_rel);
    // this will not grow!
        DBG2(std::cout << "#" << computed_hashes << " actual: " << my_rel << "\n");;

        if (X->childcount > 0)
        {
            const relative_ptr_t *children = X->getChildren();
            for(int ch = 0;ch < X->childcount;ch++)
            {
                assert(children[ch] > 0);
                SN.node_links[filled_links++] = my_rel + children[ch];          
                DBG(XML_Item *Y = reinterpret_cast<XML_Item*>(data+my_rel+children[ch]));
                DBG(std::cout << "(" << computed_hashes << "/" << total_node_count << "): " 
                    << getNodeInfo(X) << "[" << ch << "/" << X->childcount << "] = ");
                DBG(std::cout << getNodeInfo(Y) << " ----> (" << (filled_links-1) << "/" << total_node_count << ")\n");
            }
                
        }
        
        SHA1(reinterpret_cast<const unsigned char*>(X),X->length,reinterpret_cast<unsigned char*>(SN.node_hashes[computed_hashes++]));
        DBG(std::cout << "(" << computed_hashes-1 << "/" << total_node_count << "): " 
                << getNodeInfo(X) << " =====> " << SN.node_hashes[computed_hashes-1] << "/" << X->length << " B\n");
    }
    DBG(std::cout << "==================" << computed_hashes << " OK\n");

// sort all
    SN.index = reinterpret_cast<int*>(alloca(sizeof(int)*total_node_count));
    for(int i = 0;i < total_node_count;i++) SN.index[i] = i;
    mergesort_r2
       (reinterpret_cast<void*>(SN.index),
        total_node_count,sizeof(int),_SortingNodes::compare,
        reinterpret_cast<void*>(&SN)
    );
    DBG(std::cout << "\nShow all sorted " << total_node_count << " nodes:\n");
// debug show all
    DBG2(

    hash192_t prev_hash;
    memset(prev_hash,0,sizeof(prev_hash));

    int prev_addr = 0;

    for(int i = 0;i < total_node_count;i++)
    {
        assert(SN.index[i] >= 0 && SN.index[i] < total_node_count);

        DBG(std::cout << "i[" << i << "]: ID=" << SN.index[i] << "/" << total_node_count << " H=" << SN.node_hashes[SN.index[i]]);

        

        relative_ptr_t offset = SN.node_links[SN.index[i]];
        assert(offset >= 0 && offset < dst_file_size);

        XML_Item *X = reinterpret_cast<XML_Item*>(data+offset);
        DBG(std::cout << " N=" << getNodeInfo(X) << "\n");
        
    // check correct order
        int cmp1 = memcmp(prev_hash,SN.node_hashes+SN.index[i],sizeof(prev_hash));
        assert(cmp1 <= 0);
        if (cmp1 != 0)
        {
            memcpy(prev_hash,SN.node_hashes+SN.index[i],sizeof(prev_hash));
        }
        else
        {
            assert(prev_addr < SN.node_links[SN.index[i]]);
        }
        prev_addr = SN.node_links[SN.index[i]];
    }
    )
// filling _XML_Reference
// ok:

// 1. I will process all from top level (first == 0).
    int first = 0;
    hash192_t *ph = &SN.node_hashes[SN.index[first]];   // previous hash
    for(int i = 1;i < total_node_count;i++) // skipping the first record - that cannot be referenced :-)
    {
// 2. I will go through whole index[]
        assert(SN.index[i] >= 0 && SN.index[i] < total_node_count);

// 3. I will compare hash with previous hash        
        hash192_t *h = &SN.node_hashes[SN.index[i]];    // actual hash
        if (memcmp(h,ph,sizeof(hash192_t)) == 0)    // it is copy!
        {
// 4. If there is the same hash, then I will degrade XML_Item to _XML_Reference and will fill _XML_Reference::reference to original
            relative_ptr_t actual = SN.node_links[SN.index[i]];

            _XML_Reference *R = reinterpret_cast<_XML_Reference*>(data+actual);

            R->name = XTNR_REFERENCE;   // this is reference
            R->reference = SN.node_links[SN.index[first]] - actual;
            DBG(std::cout << "#" << actual << " IS REFERENCE TO idx[" << first << "]: #" << SN.node_links[SN.index[first]] << " ... = relative "  << R->reference << "\n");
            
            DBG2(std::cout << "\nFIRST " << SN.index[first] << "/" << SN.node_links[SN.index[first]] << " COPY " << SN.index[i] << "/[" << SN.node_links[SN.index[i]]<< "]       *" 
            << (R->reference >= 0 ? ANSI_RED_BRIGHT "R->reference >= 0!!!" : " R->reference=") << R->reference << ANSI_RESET_LF
            << std::flush;)
            assert(R->reference < 0); // should always be in backward direction
        }
        else
        {
            first = i;
            ph = h;
        }
    }
    delete [] SN.node_links;
    delete [] SN.node_hashes;
}

char *Bin_xml_packer::PackNode(char **_wp,char *_X)
// I will properly process 1 node
// 1. if node is reference, I will return address of referenced node
// 2. if node is not reference, I will copy all data

// *_wp is pointer to new writing position in data2 space
// _X is pointer to XML_Item in source data space
// _x is pointer to XML_Item in destionation data2 space
{
    AA(*_wp);
    char *_x = *_wp;
    
    XML_Item *X_src = reinterpret_cast<XML_Item*>(_X);
    XML_Item *X_dst = reinterpret_cast<XML_Item*>(_x);

    DBG(if (X_src->name == XTNR_PARAM_SYMBOLS)
        std::cout << "PARAM_SYMBOLS!\n");
    
    DBG(std::cout << "#" << (_X-data) << "/" << (*_wp-data2) << " PackNode: " << getNodeName(X_src->name));

    assert(X_src != nullptr);
    assert(X_src->name != XTNR_PROCESSED); // only 1 call per whole run!

    if (X_src->name == XTNR_REFERENCE)
    {
        _XML_Reference *R_src = reinterpret_cast<_XML_Reference*>(_X);
        relative_ptr_t ref = R_src->reference;
        DBG(std::cout << " ------> SRC." << (ref+(_X-data)) << "\n");
        _XML_Reference *R2_src = reinterpret_cast<_XML_Reference*>(_X + ref);
        assert(R2_src->name == XTNR_PROCESSED); // must be processed now
        return this->data2 + R2_src->reference;
    }
// plain virgin copy
    memcpy(reinterpret_cast<void*>(*_wp),reinterpret_cast<void*>(X_src),X_src->length);

    if (X_src->childcount > 0)
    {   
        *_wp += sizeof(XML_Item);
        *_wp += X_src->paramcount * sizeof(XML_Param_Description);
    // array of children links
        relative_ptr_t *children = reinterpret_cast<relative_ptr_t *>(*_wp);
        relative_ptr_t *src_children = reinterpret_cast<relative_ptr_t *>(*_wp+(_X-_x));

//      std::cout << "src_children[0/" << X_src->childcount << "] = " << src_children[0] << "\n";

        *_wp = _x + src_children[0]; // address of the 1st child is place to write <<<<<<<<<<<<<<<<<<< tady to padá
        DBG(std::cout << " {\n");
        for(int ch = 0;ch < X_src->childcount;ch++)
        {
            children[ch] = PackNode(_wp,_X+src_children[ch]) - _x;
            assert((children[ch] & 3) == 0);
        }
        DBG(std::cout << "}");
    }
    else
        *_wp += X_src->length; // total copy


    _XML_Reference *R_src = reinterpret_cast<_XML_Reference*>(_X);
    R_src->name = XTNR_PROCESSED;
    R_src->reference = _x - this->data2;

    X_dst->length = *_wp - _x;
    DBG(std::cout << " ... " << X_dst->length << "B\n");
    return _x;
}

Bin_xml_packer::Bin_xml_packer(const char *src,const char *dst)
    : Bin_xml_creator(src,dst)
{
    XML_Item A;
    _XML_Reference B;

    assert(sizeof(A) == sizeof(B));
    assert((char*)&A-(char*)&A.length == (char*)&B-(char*)&B.length); 
}

Bin_xml_packer::Bin_xml_packer(Bin_src_plugin *src,const char *dst)
    : Bin_xml_creator(src,dst)
{
    XML_Item A;
    _XML_Reference B;

    assert(sizeof(A) == sizeof(B));
    assert((char*)&A-(char*)&A.length == (char*)&B-(char*)&B.length); 
}

bool Bin_xml_packer::Convert(const char *src,const char *dst)
{
    Bin_xml_packer XC(src,dst);
    return XC.DoAll();
}

bool Bin_xml_packer::Convert(Bin_src_plugin *src,const char *dst)
{
    Bin_xml_packer XC(src,dst);
    return XC.DoAll();
}

bool Bin_xml_packer::GetXB(const char *xml_filename,char *xb_filename,const char **err_message)
{
    *err_message = nullptr;

    int len = strlen(xml_filename);
    // XML extension check
    if (len < 4 || strcmp(xml_filename+len-4,".xml") != 0)
    {
        *err_message = "expected file with .xml extension!";
        return false;
    }
    // XB creation
    strcpy(xb_filename,xml_filename);
    xb_filename[len-1] = 0;
    xb_filename[len-2] = 'b';

    Bin_xml_packer BXP(xml_filename,xb_filename);

    if (BXP.DoAll()) return true;

    *err_message = "cannot convert .xml to .xb!";
    return false;
}

} // pklib_xml::
