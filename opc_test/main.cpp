#include "file_manager.h"
#include "security_config.h"
#include <csignal>
#include <cstring>
#include <iostream>

/* * Initialize the structs with default values.
 * We leave persistPath empty here because C++ does not allow
 * direct pointer-to-array assignment in this syntax.
 */
/* Initialize two separate states with different paths */
static FileState MenuState = { .buffer = NULL, .bufferSize = 0, .filePos = 0, .isOpen = false,
                              .persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/Menu.txt" };

static FileState logState    = { .buffer = NULL, .bufferSize = 0, .filePos = 0, .isOpen = false,
                             .persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/system_logs.txt" };

static FileState firmwareState    = { .buffer = NULL, .bufferSize = 0, .filePos = 0, .isOpen = false,
                                  .persistPath = "/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/Server_files_&_folders/firmware.bin" };

static UA_NodeId myDeviceTypeId;
static UA_NodeId myDeviceId;

int main() {
    UA_Server *server = UA_Server_new();
//    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    /* 2. LOAD SECURITY
     * This calls your 10-argument function in security_config.cpp
     */
    UA_ByteString cert = UA_BYTESTRING_NULL;
    UA_ByteString key = UA_BYTESTRING_NULL;
    UA_StatusCode retval = configureSecurity(server, &cert, &key);

    if(retval != UA_STATUSCODE_GOOD) {
        std::cerr << "Failed to configure security. Ensure certificates exist in pki/own/" << std::endl;
        UA_Server_delete(server);
        return 1;
    }

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

    /* 3. Add Three Separate File Instances */
    addFileInstance(server, myDeviceId, "MenuFile", "MyDevice_MenuFile", &MenuState);
    addFileInstance(server, myDeviceId, "LogFile",    "MyDevice_LogFile",    &logState);
    addFileInstance(server, myDeviceId, "FimwareFile",    "MyDevice_FirmwareFile",    &firmwareState);

    std::cout << "Server is running at opc.tcp://localhost:4840" << std::endl;
    std::cout << "Security Mode: Sign & Encrypt | Policy: Basic256Sha256" << std::endl;

    /* 6. RUN SERVER */
    UA_Boolean running = true;
    UA_Server_run(server, &running);

    /* 7. CLEANUP */
    if(MenuState.buffer) free(MenuState.buffer);
    if(logState.buffer) free(logState.buffer);
    if(firmwareState.buffer) free(firmwareState.buffer);

    UA_ByteString_clear(&cert);
    UA_ByteString_clear(&key);
    UA_Server_delete(server);

    return 0;
}
