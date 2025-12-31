#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

extern "C" {
#include "open62541.h"
}

typedef struct {
    UA_Byte *buffer;
    size_t  bufferSize;
    size_t  filePos;
    UA_Boolean isOpen;
    UA_Byte openMode;
    char    persistPath[256];
} FileState;

void addFileInstance(UA_Server *server, UA_NodeId parentId, const char* name,
                     const char* nodeIdStr, FileState *state);

#endif
