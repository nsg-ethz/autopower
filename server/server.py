import grpc
import json
import os
import argparse
import time
import logging
import threading
import datetime

import ipaddress
import psycopg2 as postgres
import psycopg2.extras as pgextras
import api_pb2_grpc as pbdef
import concurrent.futures as futures

import jwt
from urllib.parse import unquote as ulunquote
from google.protobuf import any_pb2
from grpc_status import rpc_status
from google.rpc import status_pb2, code_pb2
from threading import Lock, RLock, Condition
from queue import LifoQueue as qu
from collections import deque as deq
from google.protobuf.timestamp_pb2 import Timestamp
from datetime import tzinfo, timezone, datetime

logging.basicConfig(level=logging.INFO)

# shared class for communication with CMeasurementApiServicer. Represents a client
class ExternalClient():
    def __init__(self, uid):
        # set up external Client class to manage each client in a separate thread.
        self.uid = uid
        self.jobqueue = qu()  # jobs to the device
        self.responsedict = dict({})  # responses from the device
        self.responsedictlock = RLock()
        self.responsedictcv = Condition(lock=self.responsedictlock)
        self.seqnoLock = Lock()
        self.lastRequestNo = 0  # increasing request number to tie jobqueue content to responsequeue content
        # Define standard measurement settings
        self.ppDevice = "MCP1"
        self.ppSamplingInterval = "500"
        self.uploadIntervalMin = 5

    def setPpDevice(self, ppdev):
        self.ppDevice = ppdev

    def getPpDevice(self):
        return self.ppDevice

    def setPpSampleInterval(self, sampInt):
        self.ppSamplingInterval = sampInt

    def getPpSampleInterval(self):
        return self.ppSamplingInterval

    def setuploadIntervalMin(self, uploadInt):
        self.uploadIntervalMin = uploadInt

    def getuploadIntervalMin(self):
        return self.uploadIntervalMin

    def scheduleDeletion(self):
        self.jobqueue.put(None)  # add None to jobqueue to terminate the thread waiting on this

    def getMsmtSettings(self):
        sttngs = {"ppDevice": self.ppDevice, "ppSamplingInterval": self.ppSamplingInterval, "uploadIntervalMin": self.uploadIntervalMin}
        return sttngs

    def scheduleRequest(self, job):
        # adds a job to the local queue.
        self.seqnoLock.acquire()
        self.lastRequestNo = self.lastRequestNo + 1
        job.requestNo = self.lastRequestNo
        seqno = self.lastRequestNo
        self.jobqueue.put(job)
        self.seqnoLock.release()
        return seqno

    def getNextRequest(self):
        return self.jobqueue.get(block=True)  # Get last content from scheduler queue. Will block if queue is empty

    def setResponse(self, response):
        self.responsedictlock.acquire()
        self.responsedict[response.requestNo] = response
        self.responsedictcv.notify_all()
        self.responsedictlock.release()

    def responseArrived(self, requestNo):
        self.responsedictlock.acquire()
        # return true if the response for requestNo is in the response dict. False otherwise
        didArrive = False
        if requestNo in self.responsedict:
            if self.responsedict[requestNo]:
                didArrive = True

        self.responsedictlock.release()
        return didArrive

    def getResponse(self, requestNo):
        # precondition: requestNo in responsedict

        self.responsedictlock.acquire()
        if not self.responseArrived(requestNo):
            self.responsedictlock.release()
            raise ArgumentError("Cannot call getResponse() on non arrived requestNo " + requestNo)
        resp = self.responsedict[requestNo]
        self.responsedictlock.release()
        return resp

    def waitForResponseTo(self, requestNo):
        self.responsedictlock.acquire()
        while not self.responseArrived(requestNo):
            # wait_for is new in version 3.2 but docs do not specify if wait_for also returns a boolean on timeout (https://docs.python.org/3/library/threading.html#threading.Condition.wait_for)
            noTimeout = self.responsedictcv.wait(timeout=30)  # times out after 30 seconds
            if not noTimeout:
                self.responsedictlock.release()
                return False  # on timeout give back issue
        # block until requestNo has arrived

        self.responsedictlock.release()
        return True

    def purgeRequestNo(self, requestNo):
        self.responsedictlock.acquire()
        if requestNo in self.responsedict:  # only delete if existing. May not be the case if server generated error msg (e.g. timeout)
            del self.responsedict[requestNo]
        self.responsedictlock.release()

# Store state of registered clients
class ClientManager():
    loggedInClients = dict({})

    def getLoggedInClientsList(self):
        return list(self.loggedInClients.keys())
    def isInLoggedInClientsList(self, id):
        return id in self.loggedInClients.keys()
    def getNextRequestOfClient(self, clientId):
        ec = self.loggedInClients[clientId]
        return ec.getNextRequest()

    def setMsmtSettingsOfClient(self, clientId, ppdev, sampleInt, uploadInt):
        if clientId not in self.getLoggedInClientsList():
            pm = OutCommunicator()
            pm.addMessage("Cannot get settings of a non registered client." + str(clientId))
            return

        ec = self.loggedInClients[clientId]
        ec.setPpDevice(ppdev)
        ec.setPpSampleInterval(sampleInt)
        ec.setuploadIntervalMin(uploadInt)

    def getMsmtSettingsOfClient(self, clientId):
        if clientId in self.loggedInClients:
            # Measurement can start -> return measurement settings to client
            ec = self.loggedInClients[clientId]
            msettings = ec.getMsmtSettings()
            return msettings

    def addNewClient(self, clientId):
        if clientId in self.getLoggedInClientsList():
            pm = OutCommunicator()
            pm.addMessage("Re-registered already existing client. Deleting old one.")
            # we delete the old ec as it is no longer needed. This is just an optimization.
            self.loggedInClients[clientId].scheduleDeletion()

        ec = ExternalClient(clientId)
        self.loggedInClients[clientId] = ec

    def scheduleNewRequestToClient(self, clientId, job):
        ec = self.loggedInClients[clientId]
        return ec.scheduleRequest(job)  # returns requestNo

    def addNewResponseToRequest(self, clientId, response):
        if clientId not in self.getLoggedInClientsList():
            pm = OutCommunicator()
            pm.addMessage("Cannot handle response of non existing client " + clientId + ". Please issue the last requests again.")
            return

        ec = self.loggedInClients[clientId]
        ec.setResponse(response)

    def getResponseOfRequestNo(self, clientId, requestNo):

        if clientId not in self.getLoggedInClientsList():
            pm = OutCommunicator()
            errMsg = "Cannot wait on a request issued to non existing client " + clientId + ". Please try again later to check once the client has re-registered"
            pm.addMessage(errMsg)
            return False

        ec = self.loggedInClients[clientId]
        if not ec.responseArrived(requestNo):
            pm = OutCommunicator()
            errMsg = "Cannot get requestno " + str(requestNo) + " for client " + clientId + " as it did not arrive yet"
            pm.addMessage(errMsg)
            return False

        return ec.getResponse(requestNo)

    def waitOnRequestCompletion(self, clientId, requestNo):
        if clientId not in self.getLoggedInClientsList():
            pm = OutCommunicator()
            pm.addMessage("Cannot wait on a request issued to non existing client " + clientId + ". Please try again later to check once the client has re-registered")
            return False

        ec = self.loggedInClients[clientId]
        gotResponse = ec.waitForResponseTo(requestNo)
        # gotResponse is true if no timeout occurred
        return gotResponse

    def purgeResponseOfRequestNo(self, clientId, requestNo):
        if clientId not in self.getLoggedInClientsList():
            pm = OutCommunicator()
            pm.addMessage("Cannot purge request issued to non existing client " + clientId + ". Please try again later to check once the client has re-registered")
            return False
        ec = self.loggedInClients[clientId]
        ec.purgeRequestNo(requestNo)
        return True

    def addSyncRequest(self, clientId, request):
        requestNo = self.scheduleNewRequestToClient(clientId, request)
        waitSucceeded = self.waitOnRequestCompletion(clientId, requestNo)
        if not waitSucceeded:
            print("Failed to get confirmation from client. This may be a timeout error.")
            return

        response = self.getResponseOfRequestNo(clientId, requestNo)
        if response.statusCode != 0:
            print("Could not execute request successfully. Client issued error: " + response.msg)
            return

        self.purgeResponseOfRequestNo(clientId, requestNo)  # clean up

        return response


class CLI():
    def addManualRequest(self):  # issue request message from CLI
        cm = ClientManager()
        while True:
            # prepare new request
            rq = pbdef.api__pb2.srvRequest()

            k = ""
            while k not in cm.getLoggedInClientsList():
                print("Select the client you want to send a message to. Available are: " + str(cm.getLoggedInClientsList()))
                k = input("Use client with key: ")

            rq.clientUid = k
            print("Available message types are:\n0: INTRODUCE_SERVER\n1: START_MEASUREMENT\n2: STOP_MEASUREMENT\n3: REQUEST_MEASUREMENT_LIST\n4: REQUEST_MEASUREMENT_DATA\n5: REQUEST_MEASUREMENT_STATUS\n6: REQUEST_AVAILABLE_PP_DEVICE")
            mTpe = int(input("Enter message type: "))
            if mTpe == 0:
                rq.msgType = pbdef.api__pb2.srvRequestType.INTRODUCE_SERVER
            elif mTpe == 1:
                # Request list of available pp devices of this client
                availableClientsRequest = pbdef.api__pb2.srvRequest()
                availableClientsRequest.clientUid = k
                availableClientsRequest.msgType = pbdef.api__pb2.REQUEST_AVAILABLE_PP_DEVICE
                deviceRequestNo = cm.scheduleNewRequestToClient(k, availableClientsRequest)
                smpInterval = input("Enter sampling interval: ")
                uploadinterval = int(input("Enter upload interval in minutes: "))

                clList = cm.getResponseOfRequestNo(k, deviceRequestNo)
                if not clList:
                    print("Could not get ppDevice list from client. Please try again later.")
                    return

                availablePPDevicesJson = json.loads(clList.msg)
                ppDeviceNames = []
                for dev in availablePPDevicesJson:
                    ppDeviceNames.append(dev["alias"])
                ppDev = ""
                while ppDev not in ppDeviceNames:
                    ppDev = input("Enter device " + str(ppDeviceNames) + ": ")


                cm.setMsmtSettingsOfClient(k, ppDev, smpInterval, uploadinterval)
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
            elif mTpe == 6:
                rq.msgType = pbdef.api__pb2.srvRequestType.REQUEST_AVAILABLE_PP_DEVICE
            print("Response: " + str(cm.addSyncRequest(k, rq)))  # blocks until the request was fulfilled or timeout

    def getLatestMessages(self):  # get the messages from the client manager and cleans up queue of last messages
        pm = OutCommunicator()
        while True:
            print(pm.getNextMessageCli())

# communicate to other scripts and issue requests
class OutCommunicator(): 

    cliMessageQueue = qu()  # a queue saving error/status messages
    outMessageQueues = {}  # a set of queues saving error/status messages for the out communication only

    # message handling
    def addMessageCli(self, message):
        self.cliMessageQueue.put(message)

    def addMessageOut(self, message):
        timestamp = datetime.timestamp(datetime.now())
        outMsg = {"type": "message", "timestamp": timestamp, "content": message}
        for manUid in self.outMessageQueues.keys():
            outQ = self.outMessageQueues[manUid]
            outQ.put(json.dumps(outMsg))

    def addNewMessageOutListener(self, managementUid):
        # add a new queue for a listener (= connection outwards)
        self.outMessageQueues[managementUid] = qu()

    def addMessage(self, message):
        self.addMessageCli(message)
        self.addMessageOut(message)

    def getNextMessageCli(self):
        return self.cliMessageQueue.get(block=True)  # get latest message from message queue. Blocks if empty

    def getNextMessageOut(self, managementUid):
        return self.outMessageQueues[managementUid].get(block=True)

# postgres connection creation class
class pgConnectorFactory():
    def __init__ (self, host, database, user, password):
        self.host = host
        self.database = database
        self.user = user
        self.password = password
    def createConnection(self):
      return postgres.connect(host=self.host,database=self.database,user=self.user,password=self.password)

# Implementation of gRPC methods
class CMeasurementApiServicer():
    def __init__(self, pgfac, allowedMgmtClients):
        # each thread should have an own postgres connection cursor for concurrent writes
        self.pgConn = pgfac.createConnection()
        self.pgFactory = pgfac
        self.allowedMgmtClients = allowedMgmtClients

    def extractIpFromContext(self, context):
        ipPort = ulunquote(context.peer())[5:]  # remove ipv4/ipv6 prefix
        ip, sep, _ = ipPort.rpartition(":")
        # check if ip string contains "[" or "]", which would hint at an ipv6 address. The ipaddress module needs removal of those characters
        if ip.startswith("[") and ip.endswith("]"):
            ip = ip[1:-1]

        ip = ipaddress.ip_address(ip)
        if not sep:
            return None # invalid ip given
        
        return ip
    def logClientOccurance(self, clientUid, context):
        # log that this client has been seen now to populate "latest" field in DB with IP

        with self.pgConn.cursor() as curs:
            curs.execute("UPDATE clients SET last_seen = NOW(), last_known_ip = %(lastip)s WHERE client_uid = %(cluid)s;", {'cluid': clientUid, 'lastip': str(self.extractIpFromContext(context))})
            self.pgConn.commit()

    def registerClient(self, request, context):
        cmAdd = ClientManager()
        pm = OutCommunicator()
        cmAdd.addNewClient(request.uid)
        with self.pgConn.cursor() as curs:
            curs.execute("INSERT INTO clients (client_uid, last_known_ip) VALUES (%(cluid)s, %(lastip)s) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW(), last_known_ip = %(lastip)s;", {'cluid': request.uid, 'lastip': str(self.extractIpFromContext(context))})
            self.pgConn.commit()
        pm.addMessage("UID '" + request.uid + "' registered")

        cmAdd = None  # never used again. Set to none in hope that the GC removes this
        while True:
            cm = ClientManager()
            req = cm.getNextRequestOfClient(request.uid)
            if req == None:
                # Terminate on the server side if there is a new registration with the same name. This should only happen if the same client with the server ID re-registered --> we end this call as it's no longer valid. This may or may never happen if GC is nice
                break
            yield req  # send the request to the client

    def putClientResponse(self, resp, context):
        # get responses from the client. This should always be a response to some request of the server
        cm = ClientManager()
        # if we get back a ping message, overwrite content with ip/peer of client
        if resp.msgType == pbdef.api__pb2.clientResponseType.INTRODUCE_CLIENT:
            # get ip address only (see https://stackoverflow.com/a/21908815)
            ipPort = ulunquote(context.peer())[5:]  # remove ipv4/ipv6 prefix
            ip, sep, _ = ipPort.rpartition(":")
            if not sep:
                result = {"peer": None, "msg": resp.msg}
            else:
                try:
                    remoteIp = ipaddress.ip_address(ip.strip("[]"))
                    result = {"peer": str(remoteIp), "msg": resp.msg}
                except Exception as e:
                    print("Error: Could not parse ip: " + e)
                    result = {"peer": None, "msg": resp.msg}

            resp.msg = json.dumps(result)

        cm.addNewResponseToRequest(resp.clientUid, resp)
        self.logClientOccurance(resp.clientUid, context)
        return pbdef.api__pb2.nothing()

    def putMeasurementList(self, request_iterator, context):
        pm = OutCommunicator()

        for remoteMsmtName in request_iterator:
            self.logClientOccurance(remoteMsmtName.clientUid, context)
            pm.addMessage(remoteMsmtName.name)

        return pbdef.api__pb2.nothing()

    def putMeasurement(self, msmts, context):
        msmtIdSoFar = ""
        with self.pgFactory.createConnection() as con:
            # own connection for a measurement upload to not conflict in any way with others.
            with con.cursor() as curs:
                curs.execute("""
                    PREPARE insMsmtData (VARCHAR(255), INT, TIMESTAMP, INT) AS
                        INSERT INTO measurement_data (server_measurement_id, measurement_value, measurement_timestamp)
                        VALUES(
                            (SELECT server_measurement_id FROM measurements WHERE shared_measurement_id = $1 LIMIT 1) -- handles only the server id
                        ,$2,$3) RETURNING $4 AS sampleid;
                """)

                mmlist = []
                for mmt in msmts:
                    # We need to tell the python Datetime that this is in UTC (see https://github.com/protocolbuffers/protobuf/issues/5910)
                    mmtTime = mmt.msmtTime.ToDatetime(tzinfo=timezone.utc)
                    if msmtIdSoFar != mmt.msmtId:
                        # optimization to just insert once we have a guaranteed different measurement id. If the ID already exists due to an outdated measurement, we just silently do nothing.
                        # This will even insert new samples if the client_run is officially finished (there's a sample after the first transmitted sample in this stream with a timestamp > stop_run)
                        # until this method call is finished.
                        # Ensure that this client is in the clients relation for integrity constraints
                        curs.execute("INSERT INTO clients (client_uid) VALUES (%(cluid)s) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW();", {'cluid': mmt.clientUid})
                        curs.execute("""
                        WITH getRunIds AS (
                            SELECT runs.run_id AS run_id FROM runs, client_runs WHERE client_runs.client_uid = %(cluid)s AND runs.run_id = client_runs.run_id AND start_run <= %(msmtts)s AND (stop_run >= %(msmtts)s OR stop_run IS NULL)
                        ),
                        getRunId AS ( -- ensures that only one result can get back. Otherwise return NULL
                            SELECT CASE WHEN (COUNT(*) <> 1) THEN NULL ELSE (SELECT * FROM getRunIds LIMIT 1) END FROM getRunIds
                        )
                        INSERT INTO
                        measurements (shared_measurement_id, client_uid, run_id)
                        VALUES (
                            %(mmtid)s,
                            %(cluid)s,
                            (SELECT * FROM getRunId LIMIT 1) -- will return NULL if there is no run id else the run id
                        )
                        ON CONFLICT DO NOTHING
                        """, {'mmtid': mmt.msmtId, 'cluid': mmt.clientUid, 'msmtts': mmtTime})
                        curs.execute("SELECT server_measurement_id FROM measurements WHERE shared_measurement_id = %(sharedid)s LIMIT 1", {'sharedid': mmt.msmtId})
                        internalId = curs.fetchone()[0]
                        con.commit()
                        # TODO: Maybe request the client to submit the measurement settings to fill the DB e.g. via requesting status
                    msmtIdSoFar = mmt.msmtId

                    mmlist.append((internalId, mmt.msmtContent,mmtTime, mmt.sampleId))
                # in the normal case insert tuples just into the measurement_data table.
                # See https://stackoverflow.com/a/60443582 as information how we (ab) use RETURNING
                ackdIds = pgextras.execute_values(curs,"""
                    SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
                    WITH ists (server_measurement_id, measurement_value, measurement_timestamp, ack_id) AS (
                      VALUES %s
                    ), new_ists AS (
                        INSERT INTO measurement_data (server_measurement_id, measurement_value, measurement_timestamp, ack_id)
                        (SELECT * FROM ists) ON CONFLICT (server_measurement_id, ack_id) DO NOTHING RETURNING ack_id
                    ) SELECT ack_id FROM new_ists
                      UNION (
                        SELECT measurement_data.ack_id FROM measurement_data, ists WHERE measurement_data.server_measurement_id = ists.server_measurement_id AND measurement_data.ack_id = ists.ack_id
                      );
                        """, mmlist, fetch=True)
                con.commit()
                # acknowledge writing to DB successfully
                def createAck(id):
                    ack = pbdef.api__pb2.sampleAck()
                    ack.sampleId = id[0]
                    return ack
                yield from map(createAck, ackdIds)
                

    def getMsmtSttngsAndStart(self, request, context):
        cm = ClientManager()
        if request.uid in cm.getLoggedInClientsList():
            settings = cm.getMsmtSettingsOfClient(request.uid)

            msettings = pbdef.api__pb2.msmtSettings()
            msettings.ppDevice = settings["ppDevice"]
            msettings.ppSamplingInterval = settings["ppSamplingInterval"]
            msettings.uploadIntervalMin = settings["uploadIntervalMin"]
            msettings.clientUid = request.uid
            # log that client was seen now - since it called this method
            self.logClientOccurance(request.uid, context)

            return msettings

    def putStatusMsg(self, request, context):
        # Store an (error) message in the DB log
        cm = ClientManager()
        pc = OutCommunicator()
        if request.clientUid in cm.getLoggedInClientsList():
            if request.statusCode != 0:
                # log errors to DB
                pc.addMessage("Logging error from '" + request.clientUid + "': " + request.msg)
                with self.pgConn.cursor() as curs:
                    # may need to create client if not exists already
                    curs.execute("INSERT INTO clients (client_uid) VALUES (%(cluid)s) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW();", {'cluid': request.clientUid})
                    curs.execute("INSERT INTO logmessages (client_uid, error_code, log_message) VALUES (%(cluid)s, %(statuscode)s, %(logmsg)s)", {'cluid': request.clientUid, 'statuscode': request.statusCode, 'logmsg': request.msg})
                    self.pgConn.commit()
            else:
                pc.addMessage("[Status] from '" + request.clientUid + "': " + request.msg)
            self.logClientOccurance(request.clientUid, context)
        return pbdef.api__pb2.nothing()

    # Management messages
    def isValidMgmtClient(self, request):
        if not request or not request.mgmtId or not request.pw:
            # For invalid request always deny
            print("WARNING: Got invalid request with auth for management method. Ignoring.")
            return False
        if request.mgmtId in self.allowedMgmtClients:
            try:
                if (self.allowedMgmtClients[request.mgmtId] == None or self.allowedMgmtClients[request.mgmtId] == ""):
                    print("WARNING: '" + request.mgmtId + "' does not have a valid secret stored on the server")
                    return False
                else:
                    # Verify JWT token
                    verifiedJWT = jwt.decode(request.pw, self.allowedMgmtClients[request.mgmtId], algorithms=["HS256"])
                if verifiedJWT["mgmtId"] == request.mgmtId:
                    return True
                else:
                    print("WARNING: did not get correct name of management client from JWT token")
                    return False
            except:
                print("WARNING: Could not verify management client JWT token")
                return False
            return True
        else:
            print("WARNING: Login failed due to invalid authentication paramaters. Ignoring.")
            return False

    def getLoggedInClients(self, request, context):
        # send all logged in clients
        if not self.isValidMgmtClient(request):
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.UNAUTHENTICATED, message="This method needs authentication. Please set credentials.")))
            return
        cm = ClientManager()
        for client in cm.getLoggedInClientsList():
            cluid = pbdef.api__pb2.clientUid()
            cluid.uid = client
            yield cluid
    
    def getRegistrationStatus(self, request, context):
        if not self.isValidMgmtClient(request):
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.UNAUTHENTICATED, message="This method needs authentication. Please set credentials.")))
            return
        cm = ClientManager()
        registrationStatus = pbdef.api__pb2.registrationStatus()
        registrationStatus.clientUid = request.clientUid
        if cm.isInLoggedInClientsList(request.clientUid):
            registrationStatus.regStatus = "Registered"
        else:
            registrationStatus.regStatus = "Not registered"
        return registrationStatus

    def setMsmtSttings(self, request, context):
        if not self.isValidMgmtClient(request):
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.UNAUTHENTICATED, message="This method needs authentication. Please set credentials.")))
            return
        cm = ClientManager()
        if request.clientUid not in cm.getLoggedInClientsList():
            # construct error via GRPC error handling
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.NOT_FOUND, message="The provided client UID wasn't registered on the server.")))
            return pbdef.api__pb2.nothing()

        # setMsmtSettingsOfClient(clientId, ppdev, sampleInt, uploadInt)
        cm.setMsmtSettingsOfClient(request.clientUid, request.ppDevice, request.ppSamplingInterval, request.uploadIntervalMin)
        return pbdef.api__pb2.nothing()

    def issueRequestToClient(self, request, context):
        if not self.isValidMgmtClient(request):
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.UNAUTHENTICATED, message="This method needs authentication. Please set credentials.")))
            return
        print("Got request for client: " + str(request.clientUid))
        cm = ClientManager()
        if request.clientUid not in cm.getLoggedInClientsList():
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.NOT_FOUND, message="The provided client UID wasn't registered on the server.")))
            print("Got unknown client id. Aborting")
            return

        clientId = request.clientUid
        manageRequestNo = request.requestNo # the request number of the management client (may not be the same as the one for the external client)
        # repack management request to client request
        requestToClient = pbdef.api__pb2.srvRequest()

        requestToClient.clientUid = clientId
        requestToClient.msgType = request.msgType
        requestToClient.requestBody = request.requestBody

        requestNo = cm.scheduleNewRequestToClient(clientId, requestToClient)
        waitSucceeded = cm.waitOnRequestCompletion(clientId, requestNo)
        if not waitSucceeded:
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.DEADLINE_EXCEEDED, message="The provided client didn't acknowledge success in time. Thus no longer waiting. The client may still succeed in future.")))
            print("Got timeout error on request from external client")
            return

        response = cm.getResponseOfRequestNo(clientId, requestNo)
        cm.purgeResponseOfRequestNo(clientId, requestNo)  # clean up
        response.requestNo = manageRequestNo
        return response

    def getMessages(self, request, context):
        if not self.isValidMgmtClient(request):
            context.abort_with_status(rpc_status.to_status(status_pb2.Status(code=code_pb2.UNAUTHENTICATED, message="This method needs authentication. Please set credentials.")))
            return
        pm = OutCommunicator()
        pm.addNewMessageOutListener(request.mgmtId)
        # TODO: We may also use GRPC messages directly here.
        while context.is_active():
            message = pm.getNextMessageOut(request.mgmtId)
            messageToSend = pbdef.api__pb2.cmMCode()
            # messageToSend.clientUid = None
            messageToSend.statusCode = 0
            messageToSend.msg = message
            yield messageToSend


def serve(secrets, config, args):
    gRPCOpts = [ 
        ("grpc.keepalive_permit_without_calls", True),
        ("grpc.keepalive_time_ms", 10*60*1000),
        ("grpc.keepalive_timeout_ms", 20*1000),
        ("grpc.http2.max_ping_strikes", 0)] 
    grpcServer = grpc.server(futures.ThreadPoolExecutor(max_workers=20), options=gRPCOpts)
    if ("ssl" in secrets):
        # Read encryption
        if (not "privKeyPath" in secrets["ssl"]):
            print("ERROR: privKeyPath is not set in config file. Please see the example config and set it correctly.")
            exit(1)

        with open(secrets["ssl"]["privKeyPath"], "rb") as srvPrivKeyReader:
            privkey = srvPrivKeyReader.read()

        if (not "pubKeyPath" in secrets["ssl"]):
            print("ERROR: pubKeyPath is not set in config file. Please see the example config and set it correctly.")
            exit(1)

        with open(secrets["ssl"]["pubKeyPath"], "rb") as srvChainReader:
            certChain = srvChainReader.read()

        if (not "pubKeyCA" in secrets["ssl"]):
            print("ERROR: pubKeyCA is not set in config file. Please see the example config and set it correctly.")
            exit(1)

        with open(secrets["ssl"]["pubKeyCA"], "rb") as caReader:
            clientCa = caReader.read()

        srvCred = grpc.ssl_server_credentials(
            private_key_certificate_chain_pairs=[(privkey, certChain)],
            root_certificates=clientCa,
            require_client_auth=True)

        grpcServer.add_secure_port(config["listenOn"], srvCred)

    elif ("insecureMode" in secrets):
        print("Warning: Started server in insecure mode. Please check that the keys are set correctly in the secrets.json config file.")
        grpcServer.add_insecure_port(config["listenOn"])
    else:
        print("Error: No SSL certificates defined and not in insecure mode. Please set keys correctly in the secrets.json config file or if you are developing, enable insecureMode in secrets.json")
        exit(1)
    allowedMgmtClients = None
    if not "allowedMgmtClients" in secrets:
        print("Warning: No management clients set in secrets file. You will not be able to manage any device. Please set allowedMgmtClients!")
    else:
        allowedMgmtClients = secrets["allowedMgmtClients"]

    dbFactory = pgConnectorFactory(host=secrets["postgres"]["host"], database=secrets["postgres"]["database"], user=secrets["postgres"]["user"], password=secrets["postgres"]["password"])
    pbdef.add_CMeasurementApiServicer_to_server(CMeasurementApiServicer(dbFactory, allowedMgmtClients), grpcServer)
    # Start output CLI
    cli = CLI()
    pt = threading.Thread(target=cli.getLatestMessages)
    pt.daemon = True
    pt.start()
    clit = threading.Thread(target=cli.addManualRequest)
    if args.interactive:
        # only start CLI input if requested via interactive flag
        clit.daemon = True
        clit.start()

    grpcServer.start()
    grpcServer.wait_for_termination()  # last to exit

    if args.interactive:
        clit.join()
    pt.join()


if __name__ == '__main__':
    # get arguments
    argParser = argparse.ArgumentParser()
    argParser.add_argument("--interactive", help="Start server in interactive mode to allow managing the CLI directly", nargs="?", const=True)
    argParser.add_argument("-s", "--secrets", help="Path to secrets file", required=True)
    argParser.add_argument("-c", "--config", help="Path to config file for server", required=True)

    args = argParser.parse_args()

    with open(args.secrets) as secrFile:
        secrets = json.load(secrFile)
    with open(args.config) as configFile:
        config = json.load(configFile)

    serve(secrets, config, args)
