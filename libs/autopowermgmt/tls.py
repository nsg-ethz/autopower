import grpc


def create_secure_channel(host, port, key_path, cert_path, ca_path=None):
    with open(key_path, "rb") as f:
        private_key = f.read()

    with open(cert_path, "rb") as f:
        certificate_chain = f.read()

    credentials_kwargs = {
        "private_key": private_key,
        "certificate_chain": certificate_chain,
    }

    # IMPORTANT: only set CA if provided
    if ca_path:
        with open(ca_path, "rb") as f:
            credentials_kwargs["root_certificates"] = f.read()

    creds = grpc.ssl_channel_credentials(**credentials_kwargs)

    return grpc.secure_channel(f"{host}:{port}", creds)
