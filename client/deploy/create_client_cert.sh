#!/bin/bash

# Creates certificates for the client. The output must be signed on the server.

# source: https://github.com/grpc/grpc/issues/9593

CN="${1}"

openssl genrsa -des3 -passout pass:1111 -out client.key 4096
openssl req -new -passin pass:1111 -key client.key -out client.csr -sha512 -subj  "/C=CH/ST=Switzerland/L=Zuerich/O=ETH-Zuerich/OU=D-ITET/CN=${CN}"
openssl rsa -passin pass:1111 -in client.key -out client.key

echo "There are still two open todos:"
echo "1. Please copy the ca.cer file of the CA to the client."
echo "2. Please sign the client.csr with the CA with e.g. the following command:"
echo "openssl x509 -req -in client.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out client.cer -days 365 -sha512"
