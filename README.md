# binary_xml

C++ Library and tools, that allows conversion of XML and JSON files to binary format and very fast access to that binary representation.

This variant of binary XML is tailored for maximum speed of loading and minimal memory foot print. 
It also does some compression - repeating nodes are written as a reference to previous occurence, but this is not fully compressed format - little compression is only side-effect. Values are stored in binary representation, where >= 32bit - they are aligned to 32bit boundary.

# Limitations

Binary XML is limited to 2GB, because I use 32bit offsets for referencing everything.

This is not fully compatible with XML - I support only 1 data element in each node. This code is ok:

    <Person>
        <Name>Petr</Name>
        <Surname>Kundrata</Surname>
    </Person>
  
But this code will not be correctly saved:

    <p>Test of bad conversion via <b>pksoft72's binary_xml</b>: it will forget this content!</p>
  
Binary XML contains 2 symbol tables - one for node names and one for parameter names, each is limited to 32K records. Symbols are referenced with 16bit signed integer and negative values are reserved for internal format symbols.

It will best fit situations, when file is saved once and loaded many time. Saving is relatively 'expensive' because it need whole tree present in memory and several passes of that tree, while for loading document tree uses libxml2 and modified jasmine libraries.

Loading is done via memory mapping file. After checking all internal references is file ready to use.  

# Compare with [EXI format](https://www.w3.org/TR/exi/)

EXI format has much better compression, but it must be read sequentially and while access is slower (due decompression) - must be either transformed to some memory wasting DOM representation or processed on the fly in one or more passes.

binary\_xml, while memory mapped, can use direct access to any node and memory footprint for random accessing file is almost equal to file size.

# Structure

File contains nodes in this format:

```c++
    class XML_Item 
    {
    ...
        tag_name_id_t   name;   // 16bit identification of tag
        int16_t     paramcount; // XML_Param_Description[]
        int32_t     childcount; // relative_ptr_t[]
        int32_t     length;     // =0 will signalize infinite object
```

|Field name|Byte size|Type     |Description                                      |
|----------|:-------:|:-------:|-------------------------------------------------|
|name      | 2       |int16\_t |identification of tag                            |
|paramcount| 2       |uint16\_t|number of parameters                             |
|childcount| 4       |int32\_t |number of tag's children                         |
|length    | 4       |int32\_t |total size of element in bytes including children|
|∑         | 12      |XML\_Item|total header size                                |
|param[0].name|2|int16\_t|identification of param|
|param[0].type|2|int16\_t|code of type of parameter|
|param[0].data|4|int32\_t|relative (`((char*)XML_Item)+data`) pointer to data of parameter or data when small enough to fit in 4 bytes|
|param[1].name|2|int16\_t|
|param[1].type|2|int16\_t|
|param[1].data|4|int32\_t|
|...|
|param[paramcount-1].data|4|int32\_t|
|data.type|1|uint8\_t|type of tag's data|
|data.value|?|.......|type dependent binary representation of data|
|param0.value|?|......|type dependent binary representation of param0.value
|param1.value|?|......|type dependent binary representation of param0.value
|...|
|child[0]|12+?|XML\_Item with all data|child tag with all it's data|
|child[1]|12+?|XML\_Item with all data|child tag with all it's data|
|...|
|child[childcount-1]|12+?|XML\_Item with all data|child tag with all it's data|
|∑         | length   ||whole size of tag is written to it's length field          |

All XML\_Item values are aligned to 4 byte boundary.
Strings are not aligned, they are 0 terminated.

While converting xml to xb, there is also optimization, where all tags are checksummed and where checksum is equal to some other tag's checksum, only reference to other tag is written and no duplicite data is stored. This optimization means, that data are not stored strongly hierarchical - as copying XML\_Item+it's length cannot contain all children. There is also impossible to add `parent` link because tag can have multiple parents.

It would be possible to do this trick with params too, but it is not implemented.

---


name signed 16 bit number which can be translated via symbol table info name of tag.

paramcount is number of parameters in object as `<TEST ID="1"></TEST>` has paramcount == 1.

childcount is number of children as `<A><B/><C/></A>` object A has childcount == 2.

length is length of whole node with all its children included - when not externally referenced.

After this 12B header is table of params as array of [16bit param id|16bit param type|32 bit data reference or data itself when short enough]  - 8 bytes per 1 parameter.

After params array (which can be empty) is list of children references - 32bit reference of each child (4*childcount bytes).

Reference is 32bit signed integer as relative pointer to XML_Item object.

Immediate after children references array are data of object itself as `<Name>Petr</Name>˙.

Data are stored with type encoded in the first byte and then aligned when needed. Strings are stored as \0 terminated strings. BLOB contains 32bit size in the first aligned integer.

After Data are stored values of params and on the end are stored all chidlren.

# Building binary xml on the fly

The is working conversion of standard xml and JSON into binary xml, but when we need a xml file creation, it is better to store it binary instead of text representation with later conversion.



