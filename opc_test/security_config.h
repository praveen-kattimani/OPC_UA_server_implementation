#ifndef SECURITY_CONFIG_H
#define SECURITY_CONFIG_H

extern "C" {
#include "open62541.h"
}

UA_StatusCode configureSecurity(UA_Server *server, UA_ByteString *cert, UA_ByteString *key);

#endif
