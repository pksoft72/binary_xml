#pragma once

// idea is simple, sending messages encoded via common symbol tables
// but it is not done (2%)

/*
namespace pklib_xml
{

class MessageQueue
{
    uint32_t whole_size;    // more MessageQueues can be in a single memory mapped file
            // sizeof(MessageQueue)+sizeof(uint32_t)*buffer_size
    uint32_t write_index;
    uint32_t buffer_size;
    uint32_t buffer[MAX_THEORETICAL_BUFFER_SIZE];
};

};
*/
