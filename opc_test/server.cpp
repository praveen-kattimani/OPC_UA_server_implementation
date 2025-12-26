extern "C" {
#include "open62541.h"
}

#include <stdio.h>
#include <string.h>

/* ================= Globals ================= */

static UA_NodeId myDeviceTypeId;
static UA_NodeId myDeviceId;
static UA_NodeId configFileId;

static UA_Boolean fileIsOpen = false;   // RENAMED
static UA_Byte buffer[2048];
static size_t bufferSize = 0;
static UA_Byte openMode = 0;
static size_t filePos = 0;   // current read/write offset


/* ================= File Callbacks ================= */

static UA_StatusCode
fileOpenMethod(UA_Server*, const UA_NodeId*, void*,
               const UA_NodeId*, void*,
               const UA_NodeId*, void*,
               size_t inputSize, const UA_Variant *input,
               size_t, UA_Variant* output) {

    if(inputSize != 1)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    UA_Byte mode = *(UA_Byte*)input[0].data;

    /* Validate mode */
    if(mode & 0xF8)   // bits 3–7 must be zero
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    openMode = mode;
    fileIsOpen = true;

    printf("openMode = 0x%02X\n", openMode);

    if(openMode & 0x04) {   // EraseExisting bit
        printf("Erase executed\n");
        memset(buffer, 0, sizeof(buffer));
        bufferSize = 0;
    }

    filePos = 0;

    UA_UInt32 handle = 1;
    UA_Variant_setScalarCopy(output, &handle,
                             &UA_TYPES[UA_TYPES_UINT32]);
    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
fileWriteMethod(UA_Server*, const UA_NodeId*, void*,
                const UA_NodeId*, void*,
                const UA_NodeId*, void*,
                size_t inputSize, const UA_Variant* input,
                size_t, UA_Variant*) {

    if(!fileIsOpen || inputSize != 2)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    /* Write bit (bit 1) must be set */
    if(!(openMode & 0x02))
        return UA_STATUSCODE_BADNOTWRITABLE;

    UA_ByteString *data =
        (UA_ByteString*)input[1].data;

    if(!data || !data->data)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    memcpy(buffer + bufferSize,
           data->data,
           data->length);
    bufferSize += data->length;

    printf("Written %zu bytes (total %zu)\n",
           data->length, bufferSize);

    return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
fileReadMethod(UA_Server*, const UA_NodeId*, void*,
               const UA_NodeId*, void*,
               const UA_NodeId*, void*,
               size_t inputSize, const UA_Variant *input,
               size_t outputSize, UA_Variant *output) {

    /* Validate arguments */
    if(inputSize != 2 || outputSize != 1)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    if(!fileIsOpen)
        return UA_STATUSCODE_BADINVALIDSTATE;

    /* Enforce Read permission (bit 0) */
    if(!(openMode & 0x01))
        return UA_STATUSCODE_BADNOTREADABLE;

    UA_UInt32 handle = *(UA_UInt32*)input[0].data;
    UA_Int32  length = *(UA_Int32*) input[1].data;

    if(handle != 1 || length <= 0)
        return UA_STATUSCODE_BADINVALIDARGUMENT;

    /* End of file */
    if(filePos >= bufferSize) {
        UA_ByteString empty = UA_BYTESTRING_NULL;
        UA_Variant_setScalarCopy(output, &empty,
                                 &UA_TYPES[UA_TYPES_BYTESTRING]);
        return UA_STATUSCODE_GOOD;
    }

    /* Clamp length to remaining bytes */
    size_t remaining = bufferSize - filePos;
    size_t toRead = (length < (UA_Int32)remaining)
                        ? (size_t)length
                        : remaining;

    UA_ByteString data;
    UA_ByteString_allocBuffer(&data, toRead);
    memcpy(data.data, buffer + filePos, toRead);

    filePos += toRead;

    UA_Variant_setScalarCopy(output, &data,
                             &UA_TYPES[UA_TYPES_BYTESTRING]);

    UA_ByteString_clear(&data);

    printf("Read %zu bytes, pos=%zu\n", toRead, filePos);

    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode
fileCloseMethod(UA_Server*, const UA_NodeId*, void*,
                const UA_NodeId*, void*,
                const UA_NodeId*, void*,
                size_t, const UA_Variant*,
                size_t, UA_Variant*) {

    if(!fileIsOpen)
        return UA_STATUSCODE_BADINVALIDSTATE;

    fileIsOpen = false;

    printf("First 20 bytes: ");

    for(size_t i = 0; i < 20 && i < bufferSize; i++)
        printf("%c", buffer[i]);

    printf("\nFile size = %zu bytes\n", bufferSize);

    /* --------- PERSIST TO DISK --------- */

    const char *persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/config_upload.csv";   // change to .csv if needed

    FILE *f = fopen(persistPath, "wb");
    if(!f) {
        printf("ERROR: Failed to open %s for writing\n", persistPath);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    size_t written = fwrite(buffer, 1, bufferSize, f);
    fclose(f);

    if(written != bufferSize) {
        printf("ERROR: Incomplete save (wrote %zu of %zu bytes)\n",
               written, bufferSize);
        return UA_STATUSCODE_BADUNEXPECTEDERROR;
    }

    printf("Persisted %zu bytes to %s\n", written, persistPath);

    /* --------- RESET STATE (optional) --------- */

    filePos = 0;
    // keep buffer content in RAM if you want Read() after Close
    // otherwise clear it:
    // memset(buffer, 0, sizeof(buffer));
    // bufferSize = 0;

    return UA_STATUSCODE_GOOD;
}


/* ================= Helper ================= */

static void bindMethod(UA_Server *server,
                       UA_NodeId obj,
                       const char *name,
                       UA_MethodCallback cb) {

    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);

    bp.startingNode = obj;

    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements =
        (UA_RelativePathElement*)
        UA_Array_new(1, &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]);

    UA_RelativePathElement_init(&bp.relativePath.elements[0]);

    bp.relativePath.elements[0].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    bp.relativePath.elements[0].isInverse = false;
    bp.relativePath.elements[0].includeSubtypes = false;
    bp.relativePath.elements[0].targetName =
        UA_QUALIFIEDNAME(0, (char*)name);

    UA_BrowsePathResult r =
        UA_Server_translateBrowsePathToNodeIds(server, &bp);

    if(r.statusCode == UA_STATUSCODE_GOOD && r.targetsSize == 1) {
        UA_Server_setMethodNode_callback(
            server,
            r.targets[0].targetId.nodeId,
            cb);
    }

    /* DO NOT FREE bp.relativePath.elements */
    UA_BrowsePathResult_clear(&r);
}



/* ================= ObjectType ================= */

static void addMyDeviceType(UA_Server *server) {

    UA_ObjectTypeAttributes a =
        UA_ObjectTypeAttributes_default;
    a.displayName = UA_LOCALIZEDTEXT("", (char*)"MyDeviceType");

    myDeviceTypeId =
        UA_NODEID_STRING(1, (char*)"MyDeviceType");

    UA_Server_addObjectTypeNode(server,
                                myDeviceTypeId,
                                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                UA_QUALIFIEDNAME(1, (char*)"MyDeviceType"),
                                a, NULL, NULL);
}

/* ================= INSTANCE ================= */

static void addMyDeviceInstance(UA_Server *server) {

    UA_ObjectAttributes o =
        UA_ObjectAttributes_default;
    o.displayName = UA_LOCALIZEDTEXT("", (char*)"MyDevice");

    myDeviceId =
        UA_NODEID_STRING(1, (char*)"MyDevice");

    UA_Server_addObjectNode(server,
                            myDeviceId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, (char*)"MyDevice"),
                            myDeviceTypeId,
                            o, NULL, NULL);

    /* -------- ConfigFile INSTANCE -------- */

    UA_ObjectAttributes f =
        UA_ObjectAttributes_default;
    f.displayName = UA_LOCALIZEDTEXT("", (char*)"ConfigFile");

    configFileId =
        UA_NODEID_STRING(1, (char*)"MyDevice_ConfigFile");

    UA_Server_addObjectNode(server,
                            configFileId,
                            myDeviceId,   // <<< CORRECT PARENT
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME(1, (char*)"ConfigFile"),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_FILETYPE),
                            f, NULL, NULL);

    bindMethod(server, configFileId, "Open",  fileOpenMethod);
    bindMethod(server, configFileId, "Write", fileWriteMethod);
    bindMethod(server, configFileId, "Read", fileReadMethod);
    bindMethod(server, configFileId, "Close", fileCloseMethod);
}

/* ================= main ================= */

int main() {
    setbuf(stdout, NULL);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(
        UA_Server_getConfig(server));

    addMyDeviceType(server);
    addMyDeviceInstance(server);

    UA_Boolean running = true;
    UA_Server_run(server, &running);

    UA_Server_delete(server);
    return 0;
}
