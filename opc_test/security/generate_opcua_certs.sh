#!/bin/bash
set -e

OUT_DIR="OPC_certs/server"
mkdir -p "$OUT_DIR"

echo "Generating OPC UA Server Private Key (PEM)..."
openssl genrsa -out "$OUT_DIR/server.key.pem" 2048

echo "Generating CSR..."
openssl req -new \
  -key "$OUT_DIR/server.key.pem" \
  -out "$OUT_DIR/server.csr" \
  -config opcua_server.cnf

echo "Self-signing Certificate..."
openssl x509 -req \
  -in "$OUT_DIR/server.csr" \
  -signkey "$OUT_DIR/server.key.pem" \
  -out "$OUT_DIR/server.crt.pem" \
  -days 3650 -sha256 \
  -extensions v3_req -extfile opcua_server.cnf

echo "Converting certificate to DER (for open62541)..."
openssl x509 -in "$OUT_DIR/server.crt.pem" \
  -outform der -out "$OUT_DIR/server.crt.der"

echo "Done."
echo "Files generated:"
ls -1 "$OUT_DIR"

