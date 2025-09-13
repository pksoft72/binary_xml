# Ideas about extended usage of binary_xml

I could try to make object database based on binary_xml.

Features:

- multiple xb/xbw files
- support for locked xb files
- network interface
- mirroring changes to multiple copies
- FAST!

## Workflow A:

1. connect
2. accept schema
3. load subtree
4. modify
5. send back
6. resolve conflicts

## Workflow B:

1. connect
2. download all/selected files
3. mirror up and down changes


## Full history

We can think about data not only as a current snapshot but about whole history and snapshots at any time. This may be less effective and only sometimes it is needed.




## binary_xml place in IT

There is very fast data processing in code, when data are stored in fixed positions in memory and has native binary format.

There is slow big data transactional storage (typically SQL database), which can provide storing great amount of data and it's transaction safety.

Binary_xml is method to join both worlds on fast solution, where all compromises can be reevaluated on application level.

## Linking objects

I suppose, objects should have their ID (at least 64bit with GUID features).
There should be possible to use direct pointer (32bit) - inside single file.
There should be way to describe reference to another file.
In header of file there should be table with referenced files - filename & code.
Code will be used while referencing to remote file.

Code should be Unique in whole application.

Binary representation whould be uint64(guid)+int32(file reference)+int32(in file offset)=128bit.

    <History>
        <ReferencedFiles>
            <File Code="H1" File="2023/01/history_1.xbw"/>
            <File Code="H2" File="2023/02/history_1.xbw"/>
        </ReferencedFiles>

        <Records>
            <Event id="134567987541646">
                <Previous ref="H2-13468764564142"/>
                <Parent ref="H1-749765464645"/>
            </Event>
        </Records>
    </History>

Referencing via 32 bit offset can be unstable as object can move as it grows ...
So backup variant through uint64 guid index file should help.

## .xbw version 2

I will need parent link for easier locking.

So I can join first_child and first_attribute pointers into children.

This will allow cleaner interface.

There can be new flag - BIN_WRITE_HAS_ATTR which can indicate, whether some attribute is present.

Also BIN_WRITE_HAS_LOCK will indicate locking node.

## Strong typing

It is possible to store type of value only in symbol table.


## .xbw binary objects

It is nice to have objects with binary representation of value and type identification via int16\_t, but they are still slow when compared to standard struct.

To boost speed, it is possible to have BLOB value with 32-bit size in first 4 bytes and binary data values with fixed size and without pointers or with relative pointers.

Size field is also usable for some version management, as when structure is extended only on the end, it can be kept compatible (with some additional conditions) with older versions.

Only flaw of this is, that no VMT can be in shared data structures (pointers are not compatible when shared between processes), so no virtual methods can be used.


## Preallocated string

For some string values, which are often changed - they cound be stored as BLOB, but presented as string. Size will be BLOB limited, but the text representation will by plain text. Update of such a string should be simple by strncpy and mark end of string as ANSI.NUL

Usage: for RSMP statuses database - strings are often changed but I would like to see them in .xbw format
