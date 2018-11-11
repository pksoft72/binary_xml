#include "bin_json_plugin.h"
#include "macros.h"
#include "jsmn/jsmn.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

namespace pklib_xml {


static int jsmn_prepare_total_size(jsmntok_t *tokens,int idx)
// will fill my extension of jmntok_t - total_size
{
    int total_size = 1;
	switch (tokens[idx].type)
	{
		case JSMN_STRING:
		case JSMN_PRIMITIVE:
			break;
		case JSMN_OBJECT:
			for(int i = tokens[idx].size;i > 0;i--)
			{
				total_size += jsmn_prepare_total_size(tokens,idx+total_size);
				total_size += jsmn_prepare_total_size(tokens,idx+total_size);
			}
			break;
		case JSMN_ARRAY:
			for(int i = tokens[idx].size;i > 0;i--)
			{
				total_size += jsmn_prepare_total_size(tokens,idx+total_size);
			}
			break;
		default:
			std::cerr << "jsmn_prepare_total_size: Unknown type " << tokens[idx].type << " at element " << idx << "\n";
			_exit(1); // ERROR
	}
    return tokens[idx].total_size = total_size;
}

//-------------------------------------------------------------------------------------------------
// Takže tady máme JSON. Je to jen osekané XML.
// Tam, kde mělo XML objekt a data a jméno objektu a zanořené objekty + nadbytečné parametry,
//  tak JSON má stringy, primitiva pole a objekty s tím, že objekty mají jméno jen výjimečně a "zvenku".

// Takže převodem z JSONu získám objekty se jmény a "vykuchané" bezejmenné objekty (prvky polí).
// Současně je objektem i obsah, ale je to vlastně jen obsah a proto by neměl být objektem?

// OK, budu mít pojmenované a nepojmenované objekty. Na jejich úrovni tu konverzi dnes (20170302) dokončím.

Bin_json_plugin::Bin_json_plugin(const char *filename,Bin_xml_creator *bin_xml_creator)
	:Bin_src_plugin(filename,bin_xml_creator)
{
	this->fd = -1;
	this->source = reinterpret_cast<const char*>(MAP_FAILED);
	this->num_tokens0 = -1;	
	this->num_tokens1 = -1;	
	this->tokens = nullptr;
	this->max_str_len = 0;
	this->last_str = nullptr;
}

Bin_json_plugin::~Bin_json_plugin()
{
	if (tokens != nullptr)
	{
		delete [] tokens;
		tokens = nullptr;
	}
	if (last_str != nullptr)
	{
		delete [] last_str;
		last_str = nullptr;
	}


	if (source != MAP_FAILED && munmap(const_cast<char *>(source),file_size) == -1)
		ERRNO_SHOW(1001,"munmap",filename);

	if (fd != -1 && close(fd) == -1)
		ERRNO_SHOW(1002,"close",filename);
}

bool Bin_json_plugin::Initialize()
{
	if (!Bin_src_plugin::Initialize()) return false;

	// 1. open file & mmap	
	std::cout << "1 open & mmap ...\n";
	int fd = open(filename,O_RDONLY);
	if (fd == -1)
	{
		ERRNO_SHOW(1003,"open",filename);
		return false;
	}
	source = reinterpret_cast<const char *>(mmap(nullptr,file_size,PROT_READ,MAP_PRIVATE,fd,0));
	if (source == MAP_FAILED)
	{
		ERRNO_SHOW(1004,"mmap",filename);
		return false;
	}

	// 2. json parsing - 1st pass - counting
//	std::cout << "2 parse (1st) ...\n";
	jsmn_parser JP;

	jsmn_init(&JP);
	num_tokens0 = jsmn_parse(&JP,source,file_size,nullptr,0);
	std::cout << " ... " << num_tokens0 << " tokens";

	tokens = new jsmntok_t[num_tokens0];
	assert(tokens != nullptr);

	// 3. json parsing - 2st pass - filling
//	std::cout << "3 parse (2nd) ...\n";
	jsmn_init(&JP);
	num_tokens1 = jsmn_parse(&JP,source,file_size,tokens,num_tokens0);
	std::cout << " ... " << num_tokens1 << " tokens\n";

//	std::cout << " ... " << num_tokens0 << " tokens\n";
	std::cout << (num_tokens0 != num_tokens1 ? "EXIT" : "OK") << "\n";

	if (num_tokens0 != num_tokens1) return false; // fail!
    
    for(int i = 0;i < num_tokens0;i++)
    {
        switch(tokens[i].type)
        {
		case JSMN_STRING:
		case JSMN_PRIMITIVE:
		case JSMN_OBJECT:
		case JSMN_ARRAY:
			break;
		default:
			std::cerr << "Bin_json_plugin::Initialize() Unknown type " << tokens[i].type << " at element " << i << "\n";
            std::cerr << "Token size = " << sizeof(jsmntok_t) << "\n";
			_exit(1); // ERROR
        }
    }

	// 4. json filling total_size
//	std::cout << "prepare total_size ... " << (void*)tokens << "\n" << std::flush;
	int total_size = jsmn_prepare_total_size(tokens,0);
	if (total_size != num_tokens0) 
	{
		std::cerr << ANSI_RED_BRIGHT "Error: jsmn_prepare_total_size() ---> " << total_size << " != " << num_tokens0 << ANSI_RESET_LF;
		return false; // fail
	}
//	std::cout << ANSI_GREEN_BRIGHT "OK: jsmn_prepare_total_size() ---> " << total_size << " == " << num_tokens0 << ANSI_RESET_LF;

	// 5. max string size
	for(int i = 0;i < num_tokens1;i++)
	{
		if (tokens[i].type != JSMN_STRING && tokens[i].type != JSMN_PRIMITIVE) continue;
		int len = tokens[i].end - tokens[i].start;
		if (len > max_str_len) max_str_len = len;
	}

	last_str = new char[max_str_len+1+4];	// possible adding "Item"
	last_str[max_str_len] = '\0';

	return true;
}

void *Bin_json_plugin::getRoot()
{
	return &tokens[0];
}

const char *Bin_json_plugin::getNodeName(void *element)
// node má jméno jen pokud je v objektu
// pokud jsem v poli, tak bych měl jméno vygenerovat z názvu pole buď odstraněním posledního "s" nebo přidáním "Item"
{
	if (element == nullptr) return nullptr; // no name
	jsmntok_t *X = reinterpret_cast<jsmntok_t *>(element);

	if (X->parent < 0) return nullptr; // no name
	if (X->parent >= num_tokens0) return nullptr;
	jsmntok_t *P = &tokens[X->parent];

	if (P->type == JSMN_ARRAY)
	{
		if (P->parent < 0) return nullptr; // no name
		if (P->parent >= num_tokens0) return nullptr;
		jsmntok_t *P2 = &tokens[P->parent];

		if (P2->type == JSMN_STRING)
		{
			assert(P2->start >= 0 && P2->start < file_size);
			assert(P2->end > 0 && P2->end <= file_size);
			int len = P2->end - P2->start;
			strncpy(last_str,source + P2->start,len);
			
			last_str[len] = 0;
			if (last_str[len-1] == 's')
			{
				last_str[len-1] = '\0';
			}
			else if (len > 4 
				&& last_str[len-4] == 'L'
				&& last_str[len-3] == 'i'
				&& last_str[len-2] == 's'
				&& last_str[len-1] == 't')
			{
				last_str[len-4] = '\0';
			}
			else
			{
				last_str[len++] = 'I';
				last_str[len++] = 't';
				last_str[len++] = 'e';
				last_str[len++] = 'm';
				last_str[len++] = '\0';
			}


			return last_str;
		}
		return "__ARRAY_ITEM_NAME__";
	}
	if (P->type != JSMN_OBJECT) return nullptr; // no object, no name!

	if (X->type != JSMN_STRING) return nullptr;

	assert(X->start >= 0 && X->start < file_size);
	assert(X->end > 0 && X->end <= file_size);
	
	strncpy(last_str,source + X->start,X->end - X->start);
	last_str[X->end - X->start] = 0;
	return last_str;
}

const char *Bin_json_plugin::getNodeValue(void *element)
// <tag>value</tag> ---> returns value
// Nemám ale jasnou reprezentaci v json?
// tag => value
// hodnota je jen STRING nebo PRIMITIVE
{
	if (element == nullptr) return nullptr;
	jsmntok_t *X = reinterpret_cast<jsmntok_t *>(element);

	if (X->parent >= 0 && X->parent < num_tokens0)
	{
		jsmntok_t *P = &tokens[X->parent];
		if (P->type == JSMN_OBJECT)
			X += X->total_size;
	}
// ok, P ukazuje na hodnotu nebo podstrom
	if (X->type != JSMN_STRING && X->type != JSMN_PRIMITIVE) return nullptr; // name
	strncpy(last_str,source + X->start,X->end - X->start);
	last_str[X->end - X->start] = 0;
	return last_str;
}

void Bin_json_plugin::ForAllChildren(OnElement_t on_element,void *parent,void *userdata)
{
	if (parent == nullptr) return;
	jsmntok_t *X = reinterpret_cast<jsmntok_t *>(parent);

	if (X->parent >= 0 && X->parent < num_tokens0)
	{
		jsmntok_t *P = &tokens[X->parent];
		if (P->type == JSMN_OBJECT)
			X += X->total_size;
	}
	if (X->type == JSMN_STRING) return;
	if (X->type == JSMN_PRIMITIVE) return;
	if (X->type == JSMN_OBJECT)
	{
		int n = X->size;
		X++;
		for(int i = 0;i < n;i++)
		{
			on_element(X,userdata);
			X += X->total_size;
			X += X->total_size;
		}
	}
	else if (X->type == JSMN_ARRAY)
	{
		int n = X->size;
		X++;
		for(int i = 0;i < n;i++)
		{
			on_element(X,userdata);
			X += X->total_size;
		}
	}
}

void Bin_json_plugin::ForAllChildrenRecursively(OnElementRec_t on_element,void *parent,void *userdata,int deep)
{
	if (parent == nullptr) return;
	jsmntok_t *X = reinterpret_cast<jsmntok_t *>(parent);

	on_element(X,userdata,deep);

	if (X->parent >= 0 && X->parent < num_tokens0)
	{
		jsmntok_t *P = &tokens[X->parent];
		if (P->type == JSMN_OBJECT)
			X += X->total_size;
	}
	if (X->type == JSMN_OBJECT)
	{
		int n = X->size;
		X++;
		for(int i = 0;i < n;i++)
		{
			ForAllChildrenRecursively(on_element,X,userdata,deep+1);	// calling with name
			X += X->total_size;
			X += X->total_size;
		}
	}
	else if (X->type == JSMN_ARRAY)
	{
		int n = X->size;
		X++;
		for(int i = 0;i < n;i++)
		{
			ForAllChildrenRecursively(on_element,X,userdata,deep+1);	// calling with name
			X += X->total_size;
		}
	}

}

void Bin_json_plugin::ForAllParams(OnParam_t on_param,void *parent,void *userdata)
{
// no params
}


} // pklib_xml::
