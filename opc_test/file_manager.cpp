#include "file_manager.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static void bindMethod(UA_Server *server, UA_NodeId obj, const char *name, UA_MethodCallback cb) {
    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = obj;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = (UA_RelativePathElement*)UA_Array_new(1, &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]);
    UA_RelativePathElement_init(&bp.relativePath.elements[0]);
    bp.relativePath.elements[0].referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    bp.relativePath.elements[0].targetName = UA_QUALIFIEDNAME(0, (char*)name);

    UA_BrowsePathResult r = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if(r.statusCode == UA_STATUSCODE_GOOD && r.targetsSize == 1) {
        UA_Server_setMethodNode_callback(server, r.targets[0].targetId.nodeId, cb);
    }
    UA_BrowsePathResult_clear(&r);
}

static UA_StatusCode
fileOpenMethod(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void*,
               const UA_NodeId*, void *objectContext,
               size_t inputSize, const UA_Variant *input, size_t, UA_Variant *output) {

    FileState *fs = (FileState*)objectContext;
    if(!fs || inputSize != 1) return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_Byte mode = *(UA_Byte*)input[0].data;
    if(mode & 0xF8) return UA_STATUSCODE_BADINVALIDARGUMENT;

    fs->openMode = mode;
    fs->isOpen = true;
    fs->filePos = 0;

    /* EraseExisting logic: Clear memory if bit 2 is set */
    if(fs->openMode & 0x04) {
        if(fs->buffer) {
            free(fs->buffer);
            fs->buffer = NULL;
        }
        fs->bufferSize = 0;
    }

    UA_UInt32 handle = 1;
    UA_Variant_setScalarCopy(output, &handle, &UA_TYPES[UA_TYPES_UINT32]);
    printf("Opened file for: %s\n", fs->persistPath);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
fileWriteMethod(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void*,
                const UA_NodeId*, void *objectContext,
                size_t inputSize, const UA_Variant* input, size_t, UA_Variant*) {

    FileState *fs = (FileState*)objectContext;
    if(!fs || !fs->isOpen || inputSize != 2) return UA_STATUSCODE_BADINVALIDSTATE;
    if(!(fs->openMode & 0x02)) return UA_STATUSCODE_BADNOTWRITABLE;

    UA_ByteString *data = (UA_ByteString*)input[1].data;
    if(!data->length) return UA_STATUSCODE_GOOD;

    /* DYNAMIC ALLOCATION: Resize the buffer to fit new data */
    size_t newSize = fs->bufferSize + data->length;
    UA_Byte *newBuffer = (UA_Byte*)realloc(fs->buffer, newSize);

    if(!newBuffer) {
        printf("Error: Out of memory!\n");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }

    fs->buffer = newBuffer;
    memcpy(fs->buffer + fs->bufferSize, data->data, data->length);
    fs->bufferSize = newSize;

    printf("Written %zu bytes to %s\n", data->length, fs->persistPath);
    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode
fileReadMethod(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void*,
               const UA_NodeId*, void *objectContext,
               size_t inputSize, const UA_Variant *input, size_t, UA_Variant *output) {

    FileState *fs = (FileState*)objectContext;
    if(!fs || !fs->isOpen || inputSize != 2) return UA_STATUSCODE_BADINVALIDSTATE;
    if(!(fs->openMode & 0x01)) return UA_STATUSCODE_BADNOTREADABLE;

    UA_Int32 length = *(UA_Int32*) input[1].data;
    if(fs->filePos >= fs->bufferSize || !fs->buffer) {
        UA_ByteString empty = UA_BYTESTRING_NULL;
        UA_Variant_setScalarCopy(output, &empty, &UA_TYPES[UA_TYPES_BYTESTRING]);
        return UA_STATUSCODE_GOOD;
    }

    size_t remaining = fs->bufferSize - fs->filePos;
    size_t toRead = ((size_t)length < remaining) ? (size_t)length : remaining;

    UA_ByteString data;
    UA_ByteString_allocBuffer(&data, toRead);
    memcpy(data.data, fs->buffer + fs->filePos, toRead);
    fs->filePos += toRead;

    UA_Variant_setScalarCopy(output, &data, &UA_TYPES[UA_TYPES_BYTESTRING]);
    UA_ByteString_clear(&data);
    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode
fileCloseMethod(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void*,
                const UA_NodeId*, void *objectContext, size_t, const UA_Variant*, size_t, UA_Variant*) {

    FileState *fs = (FileState*)objectContext;
    if(!fs || !fs->isOpen) return UA_STATUSCODE_BADINVALIDSTATE;

    fs->isOpen = false;
    if(fs->buffer && fs->bufferSize > 0) {
        FILE *f = fopen(fs->persistPath, "wb");
        if(f) {
            fwrite(fs->buffer, 1, fs->bufferSize, f);
            fclose(f);
            printf("Saved %zu bytes to %s\n", fs->bufferSize, fs->persistPath);

        }
    }

    free(fs->buffer);
    fs->buffer = NULL;

    return UA_STATUSCODE_GOOD;
}

void addFileInstance(UA_Server *server, UA_NodeId parentId, const char* name, const char* nodeIdStr, FileState *state) {
    UA_ObjectAttributes f = UA_ObjectAttributes_default;
    f.displayName = UA_LOCALIZEDTEXT("", (char*)name);
    UA_NodeId nodeId = UA_NODEID_STRING(1, (char*)nodeIdStr);

    UA_Server_addObjectNode(server, nodeId, parentId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, (char*)name),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_FILETYPE),
                            f, state, NULL); // state is the context

    bindMethod(server, nodeId, "Open",  fileOpenMethod);
    bindMethod(server, nodeId, "Write", fileWriteMethod);
    bindMethod(server, nodeId, "Read",  fileReadMethod);
    bindMethod(server, nodeId, "Close", fileCloseMethod);
}
