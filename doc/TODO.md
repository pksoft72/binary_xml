# Binary XML TODO

## Indexes

I have an idea, that it would be possible to make indexes/hash tables directly linked to tag.

    <Controllers _hash_table="">
        <Controller id="123">...</Controller>
        <Controller id="124">...</Controller>
        <Controller id="125">...</Controller>
        <Controller id="126">...</Controller>
    </Controllers>

This attribute could be produced by special function = recreating of index/hash table. Standard accessing functions would update such and index/hash table.

## Write plugin

### Write plugin - level 0

This Write plugin allows authoring directly xb files - as a base xb file and some incremental additions - at the end.

### Write plugin - level 1

This should improve efectivity - tags will be transferred as tags and binary values as binary values - without conversion to string and back.

### XML database - level 2

This will be coexistence of two files - one in xb format and second in write plugin format and their seamlessly connection in binary\_xml environment.

## Generating readers

### XSD support

I need some standardized format of type of file. Examples are too easy to break and valid XSD should be better.

### JSON schema support

For JSON it would be nice to process JSON schema.

### XML database

#### Addressing

I am thinking of something like GUID, but with variable length.

I will have FILE/object/field hierarchy.

Object can have unique ID, but it's fields may live with sequence numbers? What about history?

#### History

I am thinking about objects with long live and with content history.

Such an object will have GUID as identification and in index file there will be some GUID-FileNo-Offset = 16+4+4 = 24 bytes per object. There will be versioning with greater FileNo-Offset-> later instance.

What about distributed environment with multiple instances? I would have to be able to merge objects! I would need previous fileno, previous offset and I cannot guarantee unique fileno :-(

    <Person ID="ecb6ad2b-d9c8-42c1-8506-8d8e0a01e29a" PrevInstance="1.1214" PrevInstance="2.315"/>

To avoid conflicts during merging - there would be nice to use file hash as it's name.
But I want to use files as growing and they cannot change their names so fast!.

But File can have a GUID and when sealed, it can have a hash. Both of them is enough to identify a file. But when file is not sealed, it's pointers can change! when compacting - otherwise no compacting would be possible.

There can be some conflicts while merging.


#### Query language

It would be nice to have text query language, which could be compiled into some C++-code.
But I would have to make some syntax.
It's possible to comunicate in text only, although it is not very effective.

##### Metadata

There will be some xsd as types of xml files.

    metadata/archive.xsd
    metadata/database.xb

    TYPE Archive IS XML LIMITED TO metadata/archive.xsd



There will be description of some file hierarchy.

    




## Import plugins

### BSON

This is nice format for transfer - it is more space efficient as for expression what is next, but has no symbol table = space wasting, must be processed serially.

### C3L

This is format of scenes in Crash3D engine of Enemy in Sight 3D shooter game, I had been developing in it's team for 3 years. It would be nice to check, whether xb is suitable for this application.
