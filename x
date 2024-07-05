#!/bin/bash
make &&
    gdb --args build/default/2xbw p4-tasks-00000.xml
