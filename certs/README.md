# Certificate authority
The clients authenticate with a certificate which is signed by a Certificate authority (CA) trusted by the server. Please ensure that `openssl` is installed on your machine.

## Creating a CA
To set up a CA, edit `cnConfig.sh` with your CN attribute and run `./create_certs.sh`. If asked, enter a secure password. This will create the public and private keys for the CA (`ca.key` and `ca.cer`), as well as `server.key` (private key for server) and `server.cer` (public key for server).

## Creating a management/client cert manually

To create a certificate manually (e.g. for management clients) run the following commands. You may want to adapt the content of the -subj flag. At least replace `${CN}` with the domain of your server:

```bash
openssl genrsa -des3 -out private.key 4096
openssl req -new -key private.key -out req.csr -subj "/C=CH/ST=Switzerland/L=Zuerich/O=ETH-Zuerich/OU=D-ITET/CN=${CN}"
openssl x509 -req -in req.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out public.cer -days 365 -sha512
openssl rsa -in private.key -out private.key
```

The private key is now saved without password in `private.key` and the public key is `public.cer`. Make sure to set the correct paths in the client/management configuration files.

## Signing client/management certificates
While setting up new clients or a management connection, copy the `.csr` (Certificate signing request) to this folder and sign it via e.g. `openssl x509 -req -in <clientName>.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out <clientName>.cer -days 365 -sha512`. Now copy the `<clientName>.cer` file to the client.