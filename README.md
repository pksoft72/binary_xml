# binary_xml
C++ Library and tools, that allows conversion of XML and JSON files to binary format and very fast access to that binary representation.

This variant of binary XML is tailored for maximum speed of loading and minimal memory foot print. 
It also does some compression - repeating nodes are written as a reference to previous occurence, but this is not fully compressed format - little compression is only side-effect.

# Limitations
Binary XML is limited to 2GB, because I use 32bit offsets for referencing everything.

This is not fully compatible with XML - I support only 1 data element in each node. This code is ok:

  &lt;Person&gt;
    &lt;Name&gt;Petr&lt;/Name&gt;
    &lt;Surname&gt;Kundrata&lt;/Surname&gt;
  &lt;/Person&gt;
  
But this code will not be correctly saved:
'  &lt;p&gt;Test of bad conversion via &lt;b&gt;pksoft72's binary_xml&lt;/b&gt;: it will forget this content!&lt;/p&gt;
  
Binary XML contains 2 symbol tables - one for node names and one for parameter names, each is limited to 64K records.

It will best fit situations, when file is saved once and loaded many time. Saving is relatively 'expensive' because it need whole tree present in memory and several passes of that tree, while for loading document tree uses libxml2 and modified jasmine libraries.

  
