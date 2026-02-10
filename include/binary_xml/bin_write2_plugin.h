#pragma once

// This is new variant of bin_write_plugin.
// Language: C only
// - for easy portability
// 
// Changes:
// + removing pool reference - so for working another pointer is needed
// + parent link (needed for locking and other operations)
// + first_child & first_attribute lists joined together for smaller size
// + minimal interface - type specific stuff done using bin_xml_types
// + mode (XBW_MODE_CREATING -- will skip find because it is new node and not old)
// + mode (XBW_MODE_KEEP_VALUE -- will keep value, when existed)
// Tag creations will be in BW_plugin?
// Attr operations should be part of BW_element

// Extended element can contain some additional info, like lock, count of elements.
// Data are added after base element.
// For easy accessibility, I can add these extended binary values before base element.
// But there must be some type value added and bit of flags reserved for this


#define XBW_VERSION_WITH_PARENT_WITHOUT_POOL_REFERENCE    1   

typedef struct // 20 bytes
{
    int16_t                 identification; // reference to symbol table for tags/params
    uint8_t                 flags;          // BIN_WRITE_ELEMENT_FLAG | BIN_WRITE_REMOTE_VALUE | ... 
    XML_Binary_Type_Stored  value_type;     // 8-bits: XBT_NULL,...

    // round bidirectional list - enables effective adding to last position
    BW_offset_t             parent;         // neded for locking
    BW_offset_t             first_child;    // ATTR & TAGS together - check flags for details

    BW_offset_t             next;
    BW_offset_t             prev;
} BW_ELEMENT_t;

typedef struct
{
    
} XBW_POOL_t;


