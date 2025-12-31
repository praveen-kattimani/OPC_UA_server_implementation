#!/bin/bash

# --- Configuration ---
C_COMPILER="gcc"
CPP_COMPILER="g++"
OUTPUT_NAME="secure_server"

# CRITICAL: We need both the Encryption flag AND the MbedTLS backend flag
FLAGS="-DUA_ENABLE_ENCRYPTION -DUA_ENABLE_ENCRYPTION_MBEDTLS"

echo "Starting Build Process for OPC UA Secure Server (v1.0)..."

# 1. Compile the open62541 core (C)
echo "[1/4] Compiling open62541 core..."
$C_COMPILER -std=c99 -c open62541.c -o open62541.o $FLAGS

# 2. Compile Security Config (C++)
echo "[2/4] Compiling security_config..."
$CPP_COMPILER -std=c++11 -c security_config.cpp -o security_config.o $FLAGS

# 3. Compile remaining C++ modules
echo "[3/4] Compiling application logic..."
$CPP_COMPILER -std=c++11 -c file_manager.cpp -o file_manager.o $FLAGS
$CPP_COMPILER -std=c++11 -c main.cpp -o main.o $FLAGS

# 4. Link everything together
echo "[4/4] Linking executable..."
$CPP_COMPILER main.o file_manager.o security_config.o open62541.o -o $OUTPUT_NAME \
    -lpthread -lmbedtls -lmbedx509 -lmbedcrypto

if [ $? -eq 0 ]; then
    echo "-------------------------------------------------------"
    echo "BUILD SUCCESSFUL!"
    echo "Run your server using: ./$OUTPUT_NAME"
    echo "-------------------------------------------------------"
else
    echo "BUILD FAILED. Check the error messages above."
fi
