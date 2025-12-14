#include <binary_xml/macros.h>

static ProcessDebugStatus_t local_debug_status = {0};

ProcessDebugStatus_t *debug_status = &local_debug_status; // it is better to have it in shared file somewhere
