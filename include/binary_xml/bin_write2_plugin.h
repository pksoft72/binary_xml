#pragma once

// This is new variant of bin_write_plugin.
// Changes:
// + parent link (needed for locking and other operations)
// - first_child & first_attribute lists joined together to keep size the same
// + minimal interface - type specific stuff done using bin_xml_types
// + mode (XBW_MODE_CREATING -- will skip find because it is new node and not old)
// + mode (XBW_MODE_KEEP_VALUE -- will keep value, when existed)
// Tag creations will be in BW_plugin?
// Attr operations should be part of BW_element

// Extended element can contain some additional info, like lock, count of elements.
// Data are added after base element.
// For easy accessibility, I can add these extended binary values before base element.
// But there must be some type value added and bit of flags reserved for this
