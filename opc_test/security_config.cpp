#include "security_config.h"
#include <fstream>
#include <iostream>

// Robust file loader using standard C++
static UA_StatusCode loadFileDER(const char *path, UA_ByteString *out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if(!file.is_open())
        return UA_STATUSCODE_BADINTERNALERROR;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    UA_ByteString_allocBuffer(out, size);
    if(!file.read((char*)out->data, size))
        return UA_STATUSCODE_BADINTERNALERROR;

    out->length = size;   // <-- exact length, no '\0'
    return UA_STATUSCODE_GOOD;
}


static UA_StatusCode loadFilePEM(const char *path, UA_ByteString *out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if(!file.is_open())
        return UA_STATUSCODE_BADINTERNALERROR;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    UA_ByteString_allocBuffer(out, size + 1);
    file.read((char*)out->data, size);

    out->data[size] = '\0';   // <-- needed for mbedTLS PEM parsing
    out->length = size + 1;
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode configureSecurity(UA_Server *server,
                                UA_ByteString *cert,
                                UA_ByteString *key)
{
    UA_ServerConfig *config = UA_Server_getConfig(server);

    /* Load certificate + key */
    if(loadFileDER("/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/security/OPC_certs/server/server.crt.der", cert) != UA_STATUSCODE_GOOD)
        return UA_STATUSCODE_BADINTERNALERROR;

    if(loadFilePEM("/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/security/OPC_certs/server/server.key.pem", key) != UA_STATUSCODE_GOOD)
        return UA_STATUSCODE_BADINTERNALERROR;

    UA_ByteString trusted[1];
    loadFileDER("/home/praveenk/Desktop/OPC_UA_server_implementation/opc_test/security/OPC_certs/trusted/uaexpert.der", &trusted[0]);

    /* NOW override URI + Name so they match the certificate */
    UA_ApplicationDescription_clear(&config->applicationDescription);

    config->applicationDescription.applicationUri =
        UA_STRING_ALLOC("urn:open62541.server.application");

    config->applicationDescription.applicationName =
        UA_LOCALIZEDTEXT_ALLOC("en", "My OPC UA Server");

    /* IMPORTANT: run security config FIRST */
    return UA_ServerConfig_setDefaultWithSecurityPolicies(
        config,
        4840,
        cert,
        key,
        trusted, 1,   /* trust list  */
        NULL, 0,   /* issuer list */
        NULL, 0    /* revocation  */
        );
}

