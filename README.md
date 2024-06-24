# Autopower

The autopower project aims to collect power data from network devices automatically and save the measured data on a server.

## Setting

### Measuring

A power meter is connected to a Raspberry Pi which runs a client software called `mmclient` to control the measurements. Communication with the measurement device happens through [pinpoint](https://github.com/osmhpi/pinpoint). More specifically, based on [my fork of pinpoint](https://github.com/UsualSpec/pinpoint/tree/feature/skip-workload) which includes a flag for printing measurement data without a specified workload (see [upstream PR #15](https://github.com/osmhpi/pinpoint/pull/15) for more information).

### Data storage on client

Data from pinpoint gets processed by mmclient and then saved to a local PostgreSQL database on each client. The schema can be found in client/client_db_schema.sql.

### Data upload from client

The data of the database gets uploaded periodically to the server using [grpc](https://grpc.io/). By default this happens every 5 minutes.

### Communication client/server

The client and server communicate via grpc. The protobuf API is described in proto/

## Development

### Compiling the client
See COMPILING.md in client/

### Running the server

The server side consists of multiple python scripts. See server/README.md for information.

## Deploy

To deploy clients, please see the client/ and for the server server/ folders respectively. Please make create a certificate authority to sign the certificates of autopower clients. See certs/
