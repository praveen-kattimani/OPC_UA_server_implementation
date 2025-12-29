extern "C" {
#include "open62541.h"
}

#include <stdio.h>
#include <string.h>

/* ================= File State Structure ================= */

typedef struct {
    UA_Byte buffer[4096];
    size_t  bufferSize;
    size_t  filePos;
    UA_Boolean isOpen;
    UA_Byte openMode;
    char    persistPath[256];
} FileState;

/* Initialize two separate states with different paths */
static FileState configState = { .bufferSize = 0, .filePos = 0, .isOpen = false,
                                .persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/config_upload.csv" };

static FileState logState    = { .bufferSize = 0, .filePos = 0, .isOpen = false,
                             .persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/system_logs.txt" };

static UA_NodeId myDeviceTypeId;
static UA_NodeId myDeviceId;

/* ================= Generic File Callbacks ================= */

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

    if(fs->openMode & 0x04) fs->bufferSize = 0; // EraseExisting

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
    if(fs->bufferSize + data->length > 4096) return UA_STATUSCODE_BADOUTOFMEMORY;

    memcpy(fs->buffer + fs->bufferSize, data->data, data->length);
    fs->bufferSize += data->length;

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
    if(fs->filePos >= fs->bufferSize) {
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
    FILE *f = fopen(fs->persistPath, "wb");
    if(f) {
        fwrite(fs->buffer, 1, fs->bufferSize, f);
        fclose(f);
        printf("Saved %zu bytes to %s\n", fs->bufferSize, fs->persistPath);
    }

    return UA_STATUSCODE_GOOD;
}

/* ================= Helpers & Setup ================= */

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

static void addFileInstance(UA_Server *server, UA_NodeId parentId, const char* name, const char* nodeIdStr, FileState *state) {
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

int main() {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    /* 1. Add Device Type */
    UA_ObjectTypeAttributes ta = UA_ObjectTypeAttributes_default;
    ta.displayName = UA_LOCALIZEDTEXT("", (char*)"MyDeviceType");
    myDeviceTypeId = UA_NODEID_STRING(1, (char*)"MyDeviceType");
    UA_Server_addObjectTypeNode(server, myDeviceTypeId, UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE), UA_QUALIFIEDNAME(1, (char*)"MyDeviceType"),
                                ta, NULL, NULL);

    /* 2. Add Device Instance */
    UA_ObjectAttributes oa = UA_ObjectAttributes_default;
    oa.displayName = UA_LOCALIZEDTEXT("", (char*)"MyDevice");
    myDeviceId = UA_NODEID_STRING(1, (char*)"MyDevice");
    UA_Server_addObjectNode(server, myDeviceId, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, (char*)"MyDevice"),
                            myDeviceTypeId, oa, NULL, NULL);

    /* 3. Add Two Separate File Instances */
    addFileInstance(server, myDeviceId, "ConfigFile", "MyDevice_ConfigFile", &configState);
    addFileInstance(server, myDeviceId, "LogFile",    "MyDevice_LogFile",    &logState);

    UA_Boolean running = true;
    UA_Server_run(server, &running);
    UA_Server_delete(server);
    return 0;
}
