#include <binary_xml/bin_xml.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>	// exit
#include <assert.h>
#include <iostream>

#include <binary_xml/jsmn/jsmn.h>

#include <binary_xml/macros.h>
#include <binary_xml/files.h>
#include <binary_xml/bin_xml.h>

#include <binary_xml/bin_json_plugin.h>

using namespace pklib_xml;

const char *JSMN_TYPES[JSMN_PRIMITIVE+1] = {"UNDEFINED","OBJECT","ARRAY","STRING","PRIMITIVE"};

int Traverse(std::ostream &os,const char *source,jsmntok_t *X,int deep)
{
	int child = 1;
	switch (X->type)
	{
		case JSMN_STRING:
		case JSMN_PRIMITIVE:
			if (deep >= 0) os << "\n";

			for(int t = 0;t < deep;t++) os << "\t";
			for(int i = X->start;i < X->end;i++) os << source[i];
			return 1;
		case JSMN_OBJECT:
			os << "{";
			for(int i = 0;i < X->size;i++)
			{
				child += Traverse(os,source,&X[child],deep+1);
				os << " => ";
				child += Traverse(os,source,&X[child],-deep-1);
			}
			os << "}";
			return child;
		case JSMN_ARRAY:
			if (deep >= 0) os << "\n";
			for(int t = 0;t < deep;t++) os << "\t";
			if (deep < 0) deep = -deep;
			os << "[";
			
			for(int i = 0;i < X->size;i++)
			{
				os << "\n";
				for(int t = 0;t < deep;t++) os << "\t";
				os << "#" << i;
				child += Traverse(os,source,&X[child],deep+1);
			}
			os << "\n";
			for(int t = 0;t < deep;t++) os << "\t";
			os << "]";
			
			return child;
		default:
			_exit(1); // ERROR

	}
}

void ProcessJSON(const char *filename)
{
	std::cout << filename << " ... ";

	off_t length = file_getsize(filename);
	std::cout << length << " B";
	if (length < 0) _exit(1);
	if (length == 0) _exit(2);

	int fd = open(filename,O_RDONLY);
	if (fd == -1)
	{
		ERRNO_SHOW(1012,"open",filename);
		_exit(3);
	}

	const char *source = reinterpret_cast<const char *>(mmap(nullptr,length,PROT_READ,MAP_PRIVATE,fd,0));
	if (source == MAP_FAILED)
	{
		ERRNO_SHOW(1013,"mmap",filename);
		_exit(4);
	}

	jsmn_parser JP;
	
	jsmn_init(&JP);
	int num_tokens0 = jsmn_parse(&JP,source,length,nullptr,0);
	std::cout << " ... " << num_tokens0 << " tokens";
	jsmntok_t *tokens = new jsmntok_t[num_tokens0];
	assert(tokens != nullptr);

	jsmn_init(&JP);
	int num_tokens1 = jsmn_parse(&JP,source,length,tokens,num_tokens0);
	std::cout << " ... " << num_tokens1 << " tokens\n";
	assert(num_tokens0 == num_tokens1);
//-------------------------------------------------------------------------------------------------
//	assert(tokens->size == num_tokens1);
	Traverse(std::cout,source,tokens,0);

	
//-------------------------------------------------------------------------------------------------
	delete [] tokens;
	tokens = nullptr;


	
	if (munmap(const_cast<char *>(source),length) == -1)
	{
		ERRNO_SHOW(1014,"munmap",filename);
		_exit(5);
	}

	if (close(fd) == -1)
	{
		ERRNO_SHOW(1015,"close",filename);
		_exit(6);
	}
}

void OnElementEvent(void *element,void *userdata,int deep)
{
	for(int t = 0;t < deep;t++)
		std::cout << "\t";
	Bin_json_plugin *J = reinterpret_cast<Bin_json_plugin*>(userdata);
	const char *name = J->getNodeName(element);
	if (name != nullptr)
		std::cout << name;
	else
		std::cout << "*";
	const char *value = J->getNodeValue(element);
	if (value != nullptr)
		std::cout << " = " << J->getNodeValue(element);

	std::cout << "\n";
}

int main(int argc,char **argv)
{
	for(int i = 1;i < argc;i++)
	{
//		ProcessJSON(argv[i]);
		Bin_json_plugin J(argv[i],nullptr);
		assert(J.Initialize());
//		J.ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep);
		J.ForAllChildrenRecursively(OnElementEvent,J.getRoot(),reinterpret_cast<void*>(&J),0);
		
	}
	return 0; // OK
}
