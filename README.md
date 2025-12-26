# OPC_UA_server_implementation
This repository consists of simple OPC UA server implementation for FileType object to transfer the file from client(UAExpert) to the OPC server

OPC stack used: open62541 v1.0

#Steps to run the application
Compile open62541 library with the below command
gcc -c open62541.c -o open62541.o -Wno-unused-parameter -Wno-write-strings

Compile server.cpp file with the below command to generate executable file
g++ server.cpp open62541.o -lcrypto -lpthread -o server

To launch the application run sever executable file
