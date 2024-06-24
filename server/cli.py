import grpc
import api_pb2_grpc as pbdef
import json
import threading
import argparse
from passlib.context import CryptContext
from getpass import getpass
pwdCtxt = CryptContext(schemes=["sha512_crypt"], deprecated="auto")


class CLI():
    def addManualRequest(self, stub, mgmtId, pw):  # issue request message from CLI
        mgmtAuth = pbdef.api__pb2.mgmtAuth()
        mgmtAuth.mgmtId = mgmtId
        mgmtAuth.pw = pw
        while True:
            # prepare new request
            rq = pbdef.api__pb2.mgmtRequest()
            rq.mgmtId = mgmtId
            rq.pw = pw

            k = ""

            def getLoggedInClientsList():
                loggedInClientsIt = stub.getLoggedInClients(mgmtAuth)
                loggedInClients = []
                for c in loggedInClientsIt:
                    loggedInClients.append(c.uid)
                return loggedInClients

            loggedInClientsList = getLoggedInClientsList()

            while k not in loggedInClientsList:
                print("Select the client you want to send a message to. Available are: " + str(loggedInClientsList))
                k = input("Use client with key: ")
                loggedInClientsList = getLoggedInClientsList()

            rq.clientUid = k
            print("Available message types are:\n0: INTRODUCE_SERVER\n1: START_MEASUREMENT\n2: STOP_MEASUREMENT\n3: REQUEST_MEASUREMENT_LIST\n5: REQUEST_MEASUREMENT_STATUS")
            mTpe = int(input("Enter message type: "))
            if mTpe == 0:
                rq.msgType = pbdef.api__pb2.srvRequestType.INTRODUCE_SERVER
            elif mTpe == 1:
                smpInterval = input("Enter sampling interval: ")
                ppDev = ""
                while ppDev not in ["MCP1", "MCP2", "CPU"]:
                    ppDev = input("Enter device (MCP1, MCP2, CPU): ")

                uploadinterval = int(input("Enter upload interval in minutes: "))
                msmtSettings = pbdef.api__pb2.mgmtMsmtSettings()
                msmtSettings.mgmtId = mgmtId
                msmtSettings.pw = pw
                msmtSettings.clientUid = k
                msmtSettings.ppDevice = ppDev
                msmtSettings.ppSamplingInterval = smpInterval
                msmtSettings.uploadIntervalMin = uploadinterval

                stub.setMsmtSttings(msmtSettings)
                rq.msgType = pbdef.api__pb2.srvRequestType.START_MEASUREMENT
            elif mTpe == 2:
                rq.msgType = pbdef.api__pb2.srvRequestType.STOP_MEASUREMENT
            elif mTpe == 3:
                rq.msgType = pbdef.api__pb2.srvRequestType.REQUEST_MEASUREMENT_LIST
            elif mTpe == 4:
                rq.msgType = pbdef.api__pb2.srvRequestType.REQUEST_MEASUREMENT_DATA
                remtMsmtName = input("Enter shared measurement ID: ")
                rq.requestBody = remtMsmtName
            elif mTpe == 5:
                rq.msgType = pbdef.api__pb2.srvRequestType.REQUEST_MEASUREMENT_STATUS

            try:
                resp = stub.issueRequestToClient(rq)
                if resp.statusCode != 0:
                    raise Exception("Could not execute request successfully: " + resp.msg)
            except grpc.RpcError as e:
                print("Error from server: " + e.details())
            except Exception as e:
                print("Error: " + str(e))

    def getLatestMessages(self, stub, mgmtId, pw):  # get the messages from the client manager and cleans up queue of last messages
        mgmtAuth = pbdef.api__pb2.mgmtAuth()
        mgmtAuth.mgmtId = mgmtId
        mgmtAuth.pw = pw
        for msg in stub.getMessages(mgmtAuth):
            message = json.loads(msg.msg)
            print(message["content"])


if __name__ == '__main__':
    argParser = argparse.ArgumentParser()
    argParser.add_argument("--createpassword", help="Create a password for management authentication with server", nargs="?", const=True)
    args = argParser.parse_args()

    # Create password via password hash function and print to CLI
    if args.createpassword:
        print("This allows you to create a new management password hash for the secrets file. Copy the hash into allowedMgmtClients key at the servers' secrets.json config file.")
        pw = getpass("New Password: ")
        print(pwdCtxt.hash(pw))
        exit(0)

    with open('config/cli_secrets.json') as secrFile:
        secrets = json.load(secrFile)
    with open('config/cli_config.json') as configFile:
        config = json.load(configFile)

    if (not "remoteHost" in config):
        print("ERROR: remoteHost not found in cli_config file. Please check for vailidity of the config file.")
        exit(1)
    if (not "remotePort" in config):
        print("ERROR: remotePort not found in cli_config file. Please check for vailidity of the config file.")
        exit(1)
    if (not "mgmtId" in secrets):
        print("ERROR: mgmtId is not set in cli_secrets.json. Please set an unique management client name.")
        exit(1)
    if (not "pw" in secrets):
        print("ERROR: pw is not set in cli_secrets.json. Please set a password for this client. For the server, you can create a hash by running this script with --createpassword. For the client, put your password as pw parameter in cli_secrets.json")
        exit(1)

    if (not "privKeyPath" in secrets) or (not "pubKeyPath" in secrets):
        print("WARNING: privKeyPath/pubKeyPath is not set in cli_secrets.json. Connecting insecurely.")
        channel = grpc.insecure_channel(config["remoteHost"] + ":" + config["remotePort"])
    else:
        # setup secure connection
        with open(secrets["privKeyPath"], "rb") as privKeyReader:
            privkey = privKeyReader.read()

        with open(secrets["pubKeyPath"], "rb") as chainReader:
            chain = chainReader.read()

        # optionally load CA
        clientCa = None
        if ("pubKeyCA" in secrets):
            with open(secrets["pubKeyCA"], "rb") as caReader:
                clientCa = caReader.read()

        channel = grpc.secure_channel(
            config["remoteHost"] + ": " + config["remotePort"],
            credentials=grpc.ssl_channel_credentials(private_key=privkey, certificate_chain=chain, root_certificates=clientCa)
        )

    stub = pbdef.CMeasurementApiStub(channel)
    cli = CLI()
    latestServerReaderThread = threading.Thread(target=cli.getLatestMessages, args=[stub, secrets["mgmtId"], secrets["pw"]])
    latestServerReaderThread.start()
    userCommunicatorThread = threading.Thread(target=cli.addManualRequest, args=[stub, secrets["mgmtId"], secrets["pw"]])
    userCommunicatorThread.start()
    userCommunicatorThread.join()
    latestServerReaderThread.join()
