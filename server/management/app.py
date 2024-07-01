from flask import Flask, render_template, request, Response, stream_with_context
from markupsafe import escape
import psycopg2 as postgres
import psycopg2.extras as pgextra
from psycopg2.errors import ForeignKeyViolation
import json
import api_pb2_grpc as pbdef
import grpc
import timeago
import datetime
import pytz
from tzlocal import get_localzone
import ipaddress
from datetime import datetime
import hashlib
from werkzeug.middleware.proxy_fix import ProxyFix

allowedPpDevList = ["MCP1", "MCP2", "CPU"]

# TODO: Change this to have other paths and config files than CI
with open('../config/web_secrets.json', encoding='utf-8') as secrFile:
    secrets = json.load(secrFile)
with open('../config/web_config.json', encoding='utf-8') as configFile:
    config = json.load(configFile)
if (not "remoteHost" in config):
    print("ERROR: remoteHost not found in web_config file. Please check for vailidity of the config file.")
    exit(1)
if (not "remotePort" in config):
    print("ERROR: remotePort not found in web_config file. Please check for vailidity of the config file.")
    exit(1)
if (not "mgmtId" in secrets):
    print("ERROR: mgmtId is not set in cli_secrets.json. Please set an unique management client name.")
    exit(1)
if (not "pw" in secrets):
    print("ERROR: pw is not set in web_secrets.json. Please set a password for this client. For the server, you can create a hash by running the cli.py script with --createpassword. For the client, put your password as pw parameter in web_secrets.json")
    exit(1)
if (not "ssl" in secrets):
    print("WARNING: privKeyPath/pubKeyPath is not set in web_secrets.json. Connecting insecurely.")
    channel = grpc.insecure_channel(config["remoteHost"] + ":" + config["remotePort"])
else:
    # setup secure connection
    with open(secrets["ssl"]["privKeyPath"], "rb") as privKeyReader:
        privkey = privKeyReader.read()
    with open(secrets["ssl"]["pubKeyPath"], "rb") as chainReader:
        chain = chainReader.read()

    # optionally load CA
    clientCa = None
    if ("pubKeyCA" in secrets["ssl"]):
        with open(secrets["ssl"]["pubKeyCA"], "rb") as caReader:
            clientCa = caReader.read()

    channel = grpc.secure_channel(
        config["remoteHost"] + ": " + config["remotePort"],
        credentials=grpc.ssl_channel_credentials(private_key=privkey, certificate_chain=chain, root_certificates=clientCa)
    )

stub = pbdef.CMeasurementApiStub(channel)


def createPgConnection():
    pgConnection = postgres.connect(host=secrets["postgres"]["host"], database=secrets["postgres"]["database"], user=secrets["postgres"]["user"], password=secrets["postgres"]["password"])
    globalCurs = pgConnection.cursor()
    globalCurs.execute("""
        PREPARE clientIdExists(VARCHAR(255)) AS
          SELECT 1 FROM clients WHERE client_uid = $1;
        PREPARE clientsOfThisRun(INT) AS
          SELECT client_uid FROM client_runs WHERE run_id = $1;
        PREPARE measurementsOfThisRun(INT) AS
          SELECT server_measurement_id, shared_measurement_id, client_uid FROM measurements WHERE run_id = $1;
        """)
    return pgConnection


#### Start and define flask app ####
app = Flask(__name__)

# see https://www.blopig.com/blog/2023/10/deploying-a-flask-app-part-ii-using-an-apache-reverse-proxy/
app.wsgi_app = ProxyFix(
    app.wsgi_app, x_for=1, x_proto=1, x_host=1, x_prefix=1
)

def issueRequest(deviceUid, rq, parseJSON=False):
    # parse JSON tries to parse the response as JSON
    # set auth and deviceUid settings for server
    rq.clientUid = deviceUid
    rq.mgmtId = secrets["mgmtId"]
    rq.pw = secrets["pw"]
    try:
        resp = stub.issueRequestToClient(rq)
        if resp.statusCode != 0:
            return resp.msg, 500
    except grpc.RpcError as e:
        if parseJSON:
            return {"status": "Error from server: " + e.details()}, 500
        return ("Error from server: " + e.details()), 500
    except Exception as e:
        if parseJSON:
            return {"status": "An error occurred"}, 500
        return "An error occurred", 500

    if parseJSON:
        return json.loads(resp.msg), 200
    else:
        return resp.msg, 200


def convertToLocalTime(timezoneAwareTs):
    # Convert time zone aware TS to local (server) timezone
    localtz = get_localzone()
    timestampLocal = timezoneAwareTs.astimezone(localtz).replace(tzinfo=None)
    return timestampLocal

def getTimeAgo(timestamp):
    # convert timestamp to local time - for now it is not checked if timeago supports timezones
    timestampLocal = convertToLocalTime(timestamp)
    return timeago.format(timestampLocal)
# Register this function in template
app.jinja_env.globals.update(getTimeAgo=getTimeAgo)

def getLastSeenTimeAgo(deviceUid):
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT last_seen FROM clients WHERE client_uid = %(cluid)s LIMIT 1", {'cluid': deviceUid})
        pgConnection.commit()
        activeTimestamp = pgcurs.fetchone()
        return getTimeAgo(activeTimestamp["last_seen"])


def getAllMeasurementMetadataOfDevice(deviceUid):
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        # TODO: Find proof that this SQL query is indeed correct with all left joins
        pgcurs.execute("""
            SELECT
            shared_measurement_id,
            server_measurement_id,
            measurements.run_id AS run_id,
            devices_under_test.dut_name AS dut_name,
            devices_under_test.dut_id AS dut_id
            FROM
            measurements
            LEFT JOIN runs ON measurements.run_id = runs.run_id
            LEFT JOIN devices_under_test ON runs.dut_id = devices_under_test.dut_id
            WHERE measurements.client_uid = %(cluid)s ORDER BY shared_measurement_id DESC""", {'cluid': deviceUid})
        pgConnection.commit()
        return pgcurs.fetchall()


def deviceIsRegistered(deviceUid):
    # Check if device deviceUid is registered at the server
    mgmtAuth = pbdef.api__pb2.authClientUid()
    mgmtAuth.mgmtId = secrets["mgmtId"]
    mgmtAuth.pw = secrets["pw"]
    mgmtAuth.clientUid = deviceUid
    try:
        regStatus = stub.getRegistrationStatus(mgmtAuth, timeout=10)  # may benefit from a message checking if device is in the server list instead of manual looping
    except grpc.RpcError as e:
        return False

    return regStatus.clientUid == deviceUid and regStatus.regStatus == "Registered" # return true iff server returned registered for this device


@app.route("/")
def homepage():
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT client_uid, last_seen FROM clients ORDER BY client_uid ASC")
        pgConnection.commit()
        return render_template("startpage.html", autopowerDevices=pgcurs.fetchall())


@app.route("/manageDevice/<deviceUid>")
def deviceManagementPage(deviceUid):
    return render_template("deviceManagement.html", deviceUid=deviceUid, msmtPpDevList=allowedPpDevList, lastSeen=getLastSeenTimeAgo(deviceUid), autopowerDevMeasurements=getAllMeasurementMetadataOfDevice(deviceUid))


@app.route("/getDeviceStatus/<deviceUid>")
def getDeviceStatus(deviceUid):
    # Request measurement status of this device
    if not deviceIsRegistered(deviceUid):
        return {"status": "Not registered"}, 422  # only if client is not even on the server side --> say not registered

    statusRq = pbdef.api__pb2.mgmtRequest()
    statusRq.msgType = pbdef.api__pb2.srvRequestType.REQUEST_MEASUREMENT_STATUS
    return issueRequest(deviceUid, statusRq, parseJSON=True)


@app.route("/getDeviceStatusPost", methods=["POST"])
def getDeviceStatusPost():
    devUid = request.form.get("deviceUid")
    return getDeviceStatus(devUid)


@app.route("/getLastMeasurementTimestampOfDevicePost", methods=["POST"])
def getLastMeasurementTimestampOfDevicePost():
    # Get the last timestamp from device
    devUid = request.form.get("deviceUid")
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT MAX(measurement_timestamp) FROM measurement_data JOIN measurements ON measurement_data.server_measurement_id = measurements.server_measurement_id WHERE measurements.client_uid = %(dev)s", {'dev': devUid})
        pgConnection.commit()
        lastMsmt = pgcurs.fetchone()
        if not lastMsmt:
            return "No measurement found for this id", 404

        return lastMsmt[0].isoformat()


@app.route("/downloadMeasurement/<measurementId>")
def downloadMsmt(measurementId):
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT shared_measurement_id FROM measurements WHERE server_measurement_id = %(srvmmid)s", {'srvmmid': int(measurementId)})
        pgConnection.commit()
        sharedName = pgcurs.fetchone()
        if sharedName is None:
            return "No measurement found for this id", 404
        
        # see https://stackoverflow.com/a/51464820 and https://flask.palletsprojects.com/en/2.3.x/patterns/streaming/, https://www.postgresql.org/message-id/21932.973561518%40sss.pgh.pa.us as fact that the server side cursor is per connection
        
        @stream_with_context
        def buildCsv():
            with pgConnection.cursor(name="downloadCurs") as pgServerCurs:
                pgServerCurs.execute("SELECT measurement_timestamp, measurement_value FROM measurement_data WHERE server_measurement_id = %(srvmmid)s ORDER BY measurement_timestamp ASC", {'srvmmid': int(measurementId)})
                yield 'measurement_timestamp,measurment_value\n'
                for row in pgServerCurs:
                    yield row[0].isoformat() + ',' + str(row[1]) + '\n'

        return buildCsv(), {"Content-Type": "text/csv", "Content-Disposition": "filename=" + escape(sharedName[0])}


@app.route("/manageDevice/<deviceUid>/startMeasurement", methods=["POST"])
def startMeasurementAtDevice(deviceUid):
    if not deviceIsRegistered(deviceUid):
        return "This deviceUid is currently not registered at the server", 404

    smpInterval = int(request.form.get("msmtSamplingInt"))
    uploadInterval = int(request.form.get("msmtUploadInt"))
    ppDev = request.form.get("msmtPpDevSel")

    if ppDev not in allowedPpDevList:
        return "Got invalid Output to measure", 422

    # Now build the request to the server

    msmtSettings = pbdef.api__pb2.mgmtMsmtSettings()
    msmtSettings.mgmtId = secrets["mgmtId"]
    msmtSettings.pw = secrets["pw"]
    msmtSettings.clientUid = deviceUid
    msmtSettings.ppDevice = ppDev
    msmtSettings.ppSamplingInterval = str(smpInterval)
    msmtSettings.uploadIntervalMin = uploadInterval

    # TODO: check for error msg
    stub.setMsmtSttings(msmtSettings)

    rq = pbdef.api__pb2.mgmtRequest()
    rq.msgType = pbdef.api__pb2.srvRequestType.START_MEASUREMENT

    return issueRequest(deviceUid, rq)


@app.route("/manageDevice/<deviceUid>/stopMeasurement")
def stopMeasurementAtDevice(deviceUid):
    # Check if this client is registered at the server
    if not deviceIsRegistered(deviceUid):
        return "This deviceUid is currently not registered at the server", 404

    # Now build the request to the server

    rq = pbdef.api__pb2.mgmtRequest()
    rq.msgType = pbdef.api__pb2.srvRequestType.STOP_MEASUREMENT

    return issueRequest(deviceUid, rq)


@app.route("/manageDevice/<deviceUid>/ping")
def requestIntroduceClient(deviceUid):
    # Check if this client is registered at the server
    if not deviceIsRegistered(deviceUid):
        return "This deviceUid is currently not registered at the server", 404

    # Now build the request to the server

    rq = pbdef.api__pb2.mgmtRequest()
    rq.msgType = pbdef.api__pb2.srvRequestType.INTRODUCE_SERVER
    return issueRequest(deviceUid, rq, parseJSON=True)


@app.route("/manageMeasurement/<measurementId>")
def manageMeasurement(measurementId):
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT shared_measurement_id, client_uid, measurements.run_id FROM measurements LEFT JOIN runs USING (run_id) WHERE server_measurement_id = %(srvmmid)s", {'srvmmid': int(measurementId)})
        pgConnection.commit()
        msmt = pgcurs.fetchone()
        if not msmt:
            return "No measurement found with this id", 404

        pgcurs.execute("SELECT runs.run_id AS run_id FROM runs, client_runs WHERE runs.run_id = client_runs.run_id AND client_runs.client_uid = %(clientuid)s", {'clientuid': msmt["client_uid"]})
        pgConnection.commit()
        allRuns = pgcurs.fetchall()
        allRuns.append({"run_id": None})  # Always allow run_id to be cleared by having a NULL/None field
        return render_template("measurementManagement.html", sharedMsmtId=msmt["shared_measurement_id"], clientUid=msmt["client_uid"], measurementId=measurementId, currRun=msmt["run_id"], runsOfThisDevice=allRuns)


@app.route("/manageMeasurement/<measurementId>/updateMsmt", methods=["POST"])
def updateMeasurement(measurementId):
    with createPgConnection() as pgConnection:
        measurementId = int(measurementId)
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT client_uid, shared_measurement_id FROM measurements WHERE server_measurement_id = %(srvmsmtid)s", {'srvmsmtid': measurementId})
        pgConnection.commit()
        oldMeasurement = pgcurs.fetchone()
        if not oldMeasurement:
            return "No measurement found with this id", 404

        clientUidOld = oldMeasurement[0]

        # Verify that this run_id is sound:
        # This means:
        # * The client must be linked via the client_runs table to this run already if the run_id is not None
        # * we cannot change the client_id
        # * the run_id must exist
        # * measurement samples of this measurement must stay inside of the runs' start and stop timestamp (not checked due to possible drifts)
        if request.form.get("runId") and not request.form.get("runId") == "None":
            newRunId = int(request.form.get("runId"))
            pgcurs.execute("SELECT client_uid, start_run, stop_run FROM runs JOIN client_runs USING(run_id) WHERE run_id = %(newrunid)s", {'newrunid': newRunId})
            pgConnection.commit()
            validRuns = pgcurs.fetchall()
            isValidClientUid = False
            for newRun in validRuns:
                if newRun[0] == clientUidOld:
                    isValidClientUid = True
                    break

            if not isValidClientUid:
                return "The given new run id is invalid: The client_uid of the measurement isn't linked to the newly specified run. Make sure to link the respective client to the new run.", 422
        else:
            newRunId = None

        pgcurs.execute("UPDATE measurements SET run_id = %(newrunid)s WHERE server_measurement_id = %(srvmsmtid)s", {'newrunid': newRunId, 'srvmsmtid': measurementId})
        pgConnection.commit()
        return oldMeasurement[1]


@app.route("/manageRun/<runId>")
def manageRun(runId):
    with createPgConnection() as pgConnection:
        runId = int(runId)
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT run_description, dut_id, start_run, stop_run FROM runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        run = pgcurs.fetchone()
        if not run:
            return "Invalid run id: This run id doesn't exist.", 404

        pgcurs.execute("SELECT dut_id, dut_name FROM devices_under_test")
        pgConnection.commit()
        duts = pgcurs.fetchall()

        pgcurs.execute("SELECT client_uid FROM clients")
        pgConnection.commit()
        clientIds = pgcurs.fetchall()
        pgcurs.execute("SELECT client_uid FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        clnts = pgcurs.fetchall()
        selectedClients = []
        for client in clnts:
            selectedClients.append(client["client_uid"])
        
        pgcurs.execute("SELECT shared_measurement_id, server_measurement_id FROM measurements WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        linkedMmts = pgcurs.fetchall()

        # only show delete if no measurements are linked
        showDelete = not linkedMmts

        # Manage timing

        startTime = None
        startDate = None
        if run["start_run"]:
            startTime = convertToLocalTime(run["start_run"]).strftime('%H:%M:%S.%f')[:-3]
            startDate = convertToLocalTime(run["start_run"]).strftime('%Y-%m-%d')

        stopTime = None
        stopDate = None
        if run["stop_run"]:
            stopTime = convertToLocalTime(run["stop_run"]).strftime('%H:%M:%S.%f')[:-3]
            stopDate = convertToLocalTime(run["stop_run"]).strftime('%Y-%m-%d')

        return render_template("runManagement.html", runId=runId, run=run, duts=duts, clientIds=clientIds, selectedClients=selectedClients, serverTz=get_localzone(), startTime=startTime, startDate=startDate, stopTime=stopTime, stopDate=stopDate, showDelete=showDelete, linkedMmts=linkedMmts)


@app.route("/runs")
def runOverviewPage():
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT run_id, dut_id, dut_name, run_description, start_run, stop_run FROM runs JOIN devices_under_test USING(dut_id) ORDER BY run_id ASC")
        pgConnection.commit()
        runs = pgcurs.fetchall()
        clients = {}
        measurements = {}
        for run in runs:
            runId = run["run_id"]
            pgcurs.execute("EXECUTE clientsOfThisRun(%(runid)s)", {'runid': runId})
            pgConnection.commit()
            clients[runId] = pgcurs.fetchall()
            timestampLocal = None
            if run["start_run"]:
                timestampLocal = convertToLocalTime(run["start_run"])
                timestampLocal = timeago.format(timestampLocal)
            run["start_ago"] = timestampLocal
            pgcurs.execute("EXECUTE measurementsOfThisRun(%(runid)s)", {'runid': runId})
            pgConnection.commit()
            measurements[runId] = pgcurs.fetchall()
        return render_template("runOverview.html", runs=runs, clients=clients, measurements=measurements)


@app.route("/newRun")
def newRunPage():
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT dut_id, dut_name FROM devices_under_test")
        pgConnection.commit()
        duts = pgcurs.fetchall()

        pgcurs.execute("SELECT client_uid FROM clients")
        pgConnection.commit()
        clientIds = pgcurs.fetchall()
        startTs = datetime.now()  # for convinience, set the start time already to now
        return render_template("runManagementAdd.html", duts=duts, clientIds=clientIds, serverTz=get_localzone(), startDate=startTs.strftime("%Y-%m-%d"), startTime=startTs.strftime("%H:%M:%S.%f")[:-3], runId=None, run=None, selectedClients=None, showDelete=False)


@app.route("/deleteRun/<runId>")
def deleteRun(runId):
    with createPgConnection() as pgConnection:
        runId = int(runId)
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT 1 FROM runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        if not pgcurs.fetchone():
            return "This run doesn't exist", 404
        try:
            pgcurs.execute("SELECT shared_measurement_id FROM measurements WHERE run_id = %(runid)s", {'runid': runId})
            pgConnection.commit()
            linkedMmts = []
            for mmId in pgcurs:
                linkedMmts.append(mmId[0])
            if linkedMmts != []:
                return "Cannot delete run which is still linked to a measurement. Please delete or unlink the measurement(s) " + str(linkedMmts) + " before deleting the run.", 403

            pgcurs.execute("DELETE FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
            pgcurs.execute("DELETE FROM runs WHERE run_id = %(runid)s", {'runid': runId})
            pgConnection.commit()
        except ForeignKeyViolation as fke:
            pgConnection.rollback()
            return "Cannot delete run which is still linked to a measurement. Please delete or unlink the measurement(s) before deleting the run. Error: " + str(fke), 403
        return str(runId)


@app.route("/newRun/add", methods=["POST"])
def addNewRun():
    with createPgConnection() as pgConnection:
        dutId = int(request.form.get("dut"))
        selectedClients = request.form.getlist("clientIds")
        runDescr = request.form.get("rundesc")

        pgcurs = pgConnection.cursor()

        pgcurs.execute("SELECT 1 FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
        pgConnection.commit()
        if not pgcurs.fetchone():
            return "Invalid dut id: This dut does not exist", 422

        for clientUid in selectedClients:
            pgcurs.execute("EXECUTE clientIdExists(%(cluid)s)", {'cluid': clientUid})
            pgConnection.commit()
            thisClientExists = pgcurs.fetchone()
            if not thisClientExists:  # query checks if this client id exists
                return "Invalid client selected: At least one client doesn't exist", 422

        startDate = request.form.get("startDate")
        startTime = request.form.get("startTime")
        startTs = None
        if not startDate or not startTime or startDate == "" or startTime == "":
            return "No start time/date set. Please specify start date and time.", 422
        else:
            try:
                startTs = datetime.strptime(startDate + "T" + startTime, '%Y-%m-%dT%H:%M:%S.%f')
            except ValueError:
                return "Invalid start time/date: Please specify date end time.", 422
        stopDate = request.form.get("stopDate")
        stopTime = request.form.get("stopTime")
        stopTs = None
        if not stopDate or not stopTime or stopDate == "" or stopTime == "":
            stopDate = None
            stopTime = None
        else:
            try:
                stopTs = datetime.strptime(stopDate + "T" + stopTime, '%Y-%m-%dT%H:%M:%S.%f')
            except ValueError:
                return "Invalid end time/date: Please specify date and time.", 422

        if startTs and stopTs and startTs > stopTs:
            return "Invalid time range. Please ensure that the start timestamp is earlier than the stop timestamp", 422

        if startTs:
            # if we set a start timestamp, we must ensure that no other run with unfinished stop_ts exists (= this run is not yet finished)
            pgcurs.execute("SELECT run_id FROM client_runs JOIN runs USING (run_id) WHERE client_runs.client_uid = ANY (%(clientUidsInThisRun)s) AND runs.stop_run IS NULL", {'clientUidsInThisRun': selectedClients})
            pgConnection.commit()
            conflictingRuns = pgcurs.fetchall()
            if conflictingRuns:
                errorRunsMsg = "This run cannot be saved as run(s) with ID "
                for conflict in conflictingRuns:
                    errorRunsMsg += str(conflict["run_id"]) + ","
                errorRunsMsg = errorRunsMsg[:-1] + " may still be running. Please set the stop timestamps of those runs explicitly or use other clients!"

                return errorRunsMsg, 422
        # TODO: Check if there are unfinished runs?

        # Speedup potential?: get all currently set clients, calculate difference and then only update those which are changed
        # pgcurs.execute("SELECT client_uid, client_run_id FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
        # pgConnection.commit()
        # currentClients = pgcurs.fetchall()
        # execute the update in a transaction to allow atomicity
        # with pgConnection.transaction():
        pgcurs.execute("INSERT INTO runs (dut_id, run_description, start_run, stop_run) VALUES (%(dutid)s,%(rundesc)s, %(startrun)s, %(stoprun)s) RETURNING run_id", {'dutid': dutId, 'rundesc': runDescr, 'startrun': startTs, 'stoprun': stopTs})
        pgConnection.commit()
        runIdRow = pgcurs.fetchone()
        runId = runIdRow[0]
        pgextra.execute_values(pgcurs, "INSERT INTO client_runs (client_uid, run_id) VALUES %s", map(lambda client: (client, runId), selectedClients))

        return str(runId)


@app.route("/manageRun/<runId>/updateRun", methods=["POST"])
def updateRun(runId):
    with createPgConnection() as pgConnection:
        runId = int(runId)
        dutId = int(request.form.get("dut"))
        selectedClients = request.form.getlist("clientIds")
        runDescr = request.form.get("rundesc")

        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT run_description, dut_id FROM runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        run = pgcurs.fetchone()
        if not run:
            return "Invalid run id: This run id doesn't exist.", 404

        pgcurs.execute("SELECT 1 FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
        pgConnection.commit()
        if not pgcurs.fetchone():
            return "Invalid dut id: This dut does not exist", 422

        for clientUid in selectedClients:
            pgcurs.execute("EXECUTE clientIdExists(%(cluid)s)", {'cluid': clientUid})
            pgConnection.commit()
            thisClientExists = pgcurs.fetchone()
            if not thisClientExists:  # query checks if this client id exists
                return "Invalid client selected: At least one client does not exist: " + clientUid, 422

        startDate = request.form.get("startDate")
        startTime = request.form.get("startTime")
        startTs = None
        if not startDate or not startTime or startDate == "" or startTime == "":
            return "No start time/date set. Please specify start date and time.", 422
        else:
            try:
                startTs = datetime.strptime(startDate + "T" + startTime, '%Y-%m-%dT%H:%M:%S.%f')
            except ValueError:
                return "Invalid start time/date: Please specify date end time.", 422
        stopDate = request.form.get("stopDate")
        stopTime = request.form.get("stopTime")
        stopTs = None
        if not stopDate or not stopTime or stopDate == "" or stopTime == "":
            stopDate = None
            stopTime = None
        else:
            try:
                stopTs = datetime.strptime(stopDate + "T" + stopTime, '%Y-%m-%dT%H:%M:%S.%f')
            except ValueError:
                return "Invalid end time/date: Please specify date and time.", 422

        if startTs and stopTs and startTs > stopTs:
            return "Invalid time range. Please ensure that the start timestamp is earlier than the stop timestamp", 422

        if startTs:
            # if we set a start timestamp, we must ensure that no other run with unfinished stop_ts exists (= this run is not yet finished)
            pgcurs.execute("SELECT run_id FROM client_runs JOIN runs USING (run_id) WHERE client_runs.client_uid = ANY (%(clientUidsInThisRun)s) AND runs.stop_run IS NULL AND runs.run_id <> %(thisrunid)s", {'clientUidsInThisRun': selectedClients, 'thisrunid': runId})
            pgConnection.commit()
            conflictingRuns = pgcurs.fetchall()
            if conflictingRuns:
                errorRunsMsg = "This run cannot be saved as run(s) with ID "
                for conflict in conflictingRuns:
                    errorRunsMsg += str(conflict["run_id"]) + ","
                errorRunsMsg = errorRunsMsg[:-1] + " may still be running. Please set the stop timestamps of those runs explicitly or use other clients!"

                return errorRunsMsg, 422
        # Speedup potential?: get all currently set clients, calculate difference and then only update those which are changed
        # pgcurs.execute("SELECT client_uid, client_run_id FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
        # pgConnection.commit()
        # currentClients = pgcurs.fetchall()
        # execute the update in a transaction to allow atomicity
        # with pgConnection.transaction():
        pgcurs.execute("UPDATE runs SET dut_id = %(dutid)s, run_description = %(rundesc)s, start_run = %(startrun)s, stop_run = %(stoprun)s WHERE run_id = %(runid)s", {'dutid': dutId, 'rundesc': runDescr, 'runid': runId, 'startrun': startTs, 'stoprun': stopTs})
        # Get rid of all the old clients, then re add the new one. There may be a potential to update instead of delete and re-insert. We don't have that many clients anyway, so there shouldn't be much of a performance difference
        pgcurs.execute("DELETE FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
        pgextra.execute_values(pgcurs, "INSERT INTO client_runs (client_uid, run_id) VALUES %s", map(lambda client: (client, runId), selectedClients))
        return "OK"


@app.route("/duts")
def dutOverviewPage():
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT dut_id, dut_name, device_model, ipv6_address, ipv4_address, loc, dut_owner FROM devices_under_test ORDER BY dut_name ASC")
        pgConnection.commit()
        return render_template("dutOverview.html", duts=pgcurs.fetchall())


@app.route("/newDut")
def newDutPage():
    return render_template('dutManagementAdd.html', dut=None, dutId=None, showDelete=False)


@app.route("/deleteDut/<dutId>")
def deleteDut(dutId):
    with createPgConnection() as pgConnection:
        dutId = int(dutId)
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT 1 FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
        pgConnection.commit()
        if not pgcurs.fetchone():
            return "This dut doesn't exist", 404
        try:
            pgcurs.execute("DELETE FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
            pgConnection.commit()
        except ForeignKeyViolation:
            pgConnection.rollback()
            return "Cannot delete DUT which is still linked to a measurement. Please delete the measurement before deleting the run.", 403
        return str(dutId)


@app.route("/newDut/add", methods=["POST"])
def addNewDut():
    # Check ip addresses
    try:
        userGivenIPv6 = request.form.get("ipv6address")
        if userGivenIPv6 != "" and userGivenIPv6 != None:
            ipv6Addr = ipaddress.IPv6Address(userGivenIPv6)
        else:
            ipv6Addr = None
    except ValueError:
        return "Invalid IPv6 address given. Please check the format", 422

    try:
        userGivenIPv4 = request.form.get("ipv4address")
        if userGivenIPv4 != "" and userGivenIPv4 != None:
            ipv4Addr = ipaddress.IPv4Address(userGivenIPv4)
        else:
            ipv4Addr = None
    except ValueError:
        return "Invalid IPv4 address given. Please check the format", 422

    # Set other fields to None/NULL if not filled out (= empty "")

    dutname = request.form.get("dutname")
    if not dutname or dutname == "":
        return "No name given for DUT. Please specify a name!", 422

    devicemodel = request.form.get("devicemodel")
    if not devicemodel or devicemodel == "":
        devicemodel = None

    location = request.form.get("location")
    if not location or location == "":
        location = None

    dutowner = request.form.get("dutowner")
    if not dutowner or dutowner == "":
        dutowner = None
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor()
        pgcurs.execute("INSERT INTO devices_under_test (dut_name, device_model, ipv6_address, ipv4_address, loc, dut_owner) VALUES (%(dutname)s, %(devmodel)s, %(ipv6addr)s, %(ipv4addr)s, %(location)s, %(dutowner)s) RETURNING dut_id", {
            'dutname': dutname,
            'devmodel': devicemodel,
            'ipv6addr': ipv6Addr,
            'ipv4addr': ipv4Addr,
            'location': location,
            'dutowner': dutowner
        })

        pgConnection.commit()
        dutIdRet = pgcurs.fetchone()
        if not dutIdRet:
            return "Could not create entry. Please check input parameters.", 500
        return str(dutIdRet[0])


@app.route("/manageDut/<dutId>")
def manageDut(dutId):
    dutId = int(dutId)
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor(cursor_factory=pgextra.RealDictCursor)
        pgcurs.execute("SELECT dut_name, device_model, ipv6_address, ipv4_address, loc, dut_owner FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
        pgConnection.commit()
        dut = pgcurs.fetchone()
        if not dut:
            return "Invalid dutId: This dut does not exist", 404

        return render_template("dutManagement.html", dut=dut, dutId=dutId, showDelete=True)


@app.route("/manageDut/<dutId>/update", methods=["POST"])
def updateDut(dutId):
    dutId = int(dutId)
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT 1 FROM devices_under_test WHERE dut_id = %(dutid)s", {'dutid': dutId})
        pgConnection.commit()
        if not pgcurs.fetchone():
            return "Invalid dutId: This dut does not exist", 404

        # Check ip addresses
        try:
            userGivenIPv6 = request.form.get("ipv6address")
            if userGivenIPv6 != "" and userGivenIPv6 != None:
                ipv6Addr = ipaddress.IPv6Address(userGivenIPv6)
            else:
                ipv6Addr = None
        except ValueError:
            return "Invalid IPv6 address given. Please check the format", 422

        try:
            userGivenIPv4 = request.form.get("ipv4address")
            if userGivenIPv4 != "" and userGivenIPv4 != None:
                ipv4Addr = ipaddress.IPv4Address(userGivenIPv4)
            else:
                ipv4Addr = None
        except ValueError:
            return "Invalid IPv4 address given. Please check the format", 422

        # Set other fields to None/NULL if not filled out (= empty "")

        dutname = request.form.get("dutname")
        if not dutname or dutname == "":
            return "Please specify a DUT name. The DUT name cannot be empty", 422

        devicemodel = request.form.get("devicemodel")
        if not devicemodel or devicemodel == "":
            devicemodel = None

        location = request.form.get("location")
        if not location or location == "":
            location = None

        dutowner = request.form.get("dutowner")
        if not dutowner or dutowner == "":
            dutowner = None

        # write updates to DB
        pgcurs.execute("UPDATE devices_under_test SET dut_name = %(dutname)s, device_model=%(devicemodel)s, ipv6_address=%(ipv6addr)s, ipv4_address=%(ipv4addr)s, loc=%(location)s, dut_owner=%(dutowner)s WHERE dut_id = %(dutid)s",
                       {
                           'dutid': dutId,
                           'dutname': dutname,
                           'devicemodel': devicemodel,
                           'ipv6addr': str(ipv6Addr) if ipv6Addr else None,
                           'ipv4addr': str(ipv4Addr) if ipv4Addr else None,
                           'location': location,
                           'dutowner': dutowner
                       })

        pgConnection.commit()

        return "OK"


@app.route("/finishRun/<runId>")
def finishRun(runId):
    runId = int(runId)
    with createPgConnection() as pgConnection:
        pgcurs = pgConnection.cursor()
        pgcurs.execute("SELECT stop_run FROM runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        stopRun = pgcurs.fetchone()
        if not stopRun:
            return "Invalid runId: This run does not exist", 404

        currentTs = datetime.now(pytz.utc)
        if stopRun[0] and stopRun[0] < currentTs:
            # Unclear what is to do if user wants to end a run which has already ended (= has a timestamp set). Otherwise, if the timestamp is already set but in the future, we just manually set it to the time we finish
            return "Cannot terminate run with an already completed run (end timestamp already set and in the past) for now. This feature is not yet defined", 500

        pgcurs.execute("SELECT client_uid FROM client_runs WHERE run_id = %(runid)s", {'runid': runId})
        pgConnection.commit()
        clientUidsInThisRun = pgcurs.fetchall()
        failedClientMsg = []  # log failed client messages
        for client in clientUidsInThisRun:
            msg, retcode = stopMeasurementAtDevice(client[0])
            if retcode != 200:
                # return tuple --> error ocurred
                failedClientMsg.append((client[0], msg, retcode))
        pgcurs.execute("UPDATE runs SET stop_run = %(currentts)s WHERE run_id = %(runid)s", {'currentts': currentTs.strftime('%Y-%m-%dT%H:%M:%S.%f')[:-3] + currentTs.strftime("%z"), 'runid': runId})
        pgConnection.commit()
        if failedClientMsg == []:
            return "OK"
        else:
            return "Not all clients could be stopped successfully: " + str(failedClientMsg)

@app.route("/finishRunPost", methods=["POST"])
def finishRunPost():
    runId = int(request.form.get("runId"))
    return finishRun(runId)