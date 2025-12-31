# OPC_UA_server_implementation
This repository consists of simple OPC UA server implementation for FileType object to transfer the file from client(UAExpert) to the OPC server

OPC stack : open62541 v1.0
client : UAExpert
#Steps to run the application
build the application using "build.sh" script

to generate server certificate and private key run the script "generate_opcua_certs.sh"
update the certificate and private key path inside "security_config.cpp" as per your system path

To launch the application run "secure_server" executable file
