#include <binary_xml/bin_xml_plugin.h>
#include <assert.h>

namespace pklib_xml {

Bin_xml_plugin::Bin_xml_plugin(const char *filename,Bin_xml_creator *bin_xml_creator)
    : Bin_src_plugin(filename,bin_xml_creator)
{
    LIBXML_TEST_VERSION;
    this->doc = nullptr;
    this->root_element = nullptr;
}

/*static void print_element_names(xmlNode * a_node)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) 
    {
        if (cur_node->type == XML_ELEMENT_NODE) 
        {
            printf("node type: Element, name: %s\n", cur_node->name);
        }

        print_element_names(cur_node->children);
    }
}
*/

Bin_xml_plugin::~Bin_xml_plugin()
{
    if (doc != nullptr) 
    {
        xmlFreeDoc(doc);
        doc = nullptr;
    }
    root_element = nullptr;

    xmlCleanupParser(); // není to nebezpečné?
}

bool Bin_xml_plugin::Initialize()
{
    if (!Bin_src_plugin::Initialize()) return false;
// loading xml
    doc = xmlReadFile(filename, NULL, 0);
    if (doc == nullptr)
    {
        std::cerr << "error: could not parse file " << filename << "\n";
        return false;
    }
    root_element = xmlDocGetRootElement(doc);
    if (root_element == nullptr)
    {
        std::cerr << "error: cannot find root element of file " << filename << "\n";
        return false;
    }
    return true;
}

//-------------------------------------------------------------------------------------------------

void *Bin_xml_plugin::getRoot()
{
    return root_element;
}

const char *Bin_xml_plugin::getNodeName(void *element)
{
    xmlNode *X = reinterpret_cast<xmlNode *>(element);
    return reinterpret_cast<const char*>(X->name);
}

const char *Bin_xml_plugin::getNodeValue(void *element)
{
    xmlNode *X = reinterpret_cast<xmlNode *>(element);
    if (X->children == nullptr) return nullptr;
    if (X->children->type != XML_TEXT_NODE) return nullptr;
    const char *value = reinterpret_cast<const char *>(X->children->content);
    if (value == nullptr) return nullptr;
//  if (X->childcount > 0) // is there nonspace value?
    while(*value != '\0' && isspace(*value)) value++;
    if (*value == '\0') return nullptr;
    return value;   
}
//-------------------------------------------------------------------------------------------------

void Bin_xml_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
{
    assert(on_element != nullptr);
    if (parent == nullptr) return; // done

    for(xmlNode *X = reinterpret_cast<xmlNode *>(parent)->children;X != nullptr;X = X->next)
        if (X->type == XML_ELEMENT_NODE)
            on_element(X,userdata);
}

void Bin_xml_plugin::ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep)
{
    assert(on_element != nullptr);
    if (parent == nullptr) return; // done
    on_element(reinterpret_cast<xmlNode*>(parent),userdata,deep);

    for(xmlNode *X = reinterpret_cast<xmlNode *>(parent)->children;X != nullptr;X = X->next)
        if (X->type == XML_ELEMENT_NODE)
        {
            ForAllChildrenRecursively(on_element,X,userdata,deep+1);
        }
}

void Bin_xml_plugin::ForAllParams(OnParam_t on_param,void *element,void *userdata)
{
    assert(on_param != nullptr);
    if (element == nullptr) return;
    xmlNode *X = reinterpret_cast<xmlNode *>(element);

    if (X->ns != nullptr)
    {   
        on_param("xmlns",(X->ns->href != nullptr ? reinterpret_cast<const char *>(X->ns->href) : ""),element,userdata);
    }
    else if (X->nsDef != nullptr)
    {   
        on_param("xmlns",(X->nsDef->href != nullptr ? reinterpret_cast<const char *>(X->nsDef->href) : ""),element,userdata);
    }


    for(xmlAttr *A = X->properties;A != nullptr;A = A->next)
    {
        assert(A->children->type == XML_TEXT_NODE);
        const char *value = reinterpret_cast<const char*>(A->children->content);
    
        on_param(reinterpret_cast<const char*>(A->name),value,element,userdata);
    }
}

}
