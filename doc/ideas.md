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



