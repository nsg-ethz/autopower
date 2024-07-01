#!/bin/bash
# Follows https://github.com/grpc/grpc/issues/9593#issuecomment-277946137
# Generate valid CA
source cnConfig.sh
echo "Creating certs for CA with CN ${CN}"

if [ -f ca.key ]; then
  echo "ca.key exists already. Skipping creation of CA. If you want to re-generate a CA, please delete ca.key"
  exit 1
else
  echo "Generating CA. Please input a secure password if asked"
  openssl genrsa -des3 -out ca.key 4096
  openssl req -new -nodes -x509 -days 365 -key ca.key -out ca.cer -sha512 -subj "/C=CH/ST=Switzerland/L=Zuerich/O=ETH-Zuerich/OU=D-ITET/CN=Autopower Root"
fi

# Generate valid Server Key/Cert (may in practice be replaced with e.g. letsencrypt certificate)
openssl genrsa -des3 -out server.key 4096
openssl req -new -key server.key -out server.csr -subj "/C=CH/ST=Switzerland/L=Zuerich/O=ETH-Zuerich/OU=D-ITET/CN=${CN}"
openssl x509 -req -in server.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out server.cer -days 365 -sha512

# Remove passphrase from the Server Key
openssl rsa -in server.key -out server.key
