#include "client.h"
#include "api.grpc.pb.h"
#include "api.pb.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cwchar>
#include <exception>
#include <fstream>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <memory>
#include <mutex>
#include <ostream>
#include <pqxx/pqxx>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

bool AutopowerClient::isValidPpDevice(std::string device) {
  // check if device is in the supportedDevices list
  return std::find(this->supportedDevices.begin(), this->supportedDevices.end(), device) != this->supportedDevices.end();
}

void AutopowerClient::putStatusToServer(uint32_t statuscode, std::string message) {
  grpc::ClientContext msgCtxt;
  class autopapi::nothing nth;
  autopapi::cmMCode cmCde;
  cmCde.set_statuscode(statuscode);
  cmCde.set_msg(message);
  cmCde.set_clientuid(clientUid);

  stub->putStatusMsg(&msgCtxt, cmCde, &nth);
}

void AutopowerClient::putResponseToServer(uint32_t statuscode, std::string message, autopapi::clientResponseType respType, uint32_t requestNo) {
  grpc::ClientContext responseMsgCtxt;
  class autopapi::nothing nth;
  autopapi::clientResponse responseMsg;

  responseMsg.set_statuscode(statuscode);
  responseMsg.set_msg(message);
  responseMsg.set_clientuid(clientUid);
  responseMsg.set_msgtype(respType);
  responseMsg.set_requestno(requestNo);
  stub->putClientResponse(&responseMsgCtxt, responseMsg, &nth);
}

std::string AutopowerClient::getReadableTimestamp(std::time_t rtme) {
  struct tm *msmtStartTime;
  msmtStartTime = gmtime(&rtme);
  char tbuffer[std::size("yyyy-mm-ddThh:mm:ssZ")];
  strftime(tbuffer, sizeof tbuffer, "%FT%TZ", msmtStartTime);
  return std::string(tbuffer);
}
std::string AutopowerClient::getCurrentReadableTimestamp() {
  // Get UTC timestamp. Unfortunately std::chrono is more complicated. Hence
  // use time_t.
  std::time_t rtme = std::time(nullptr);
  return getReadableTimestamp(rtme);
}

void AutopowerClient::setMsmtIdToNow() {
  // sets the measurement file name to now
  std::random_device rd;
  std::uniform_int_distribution<int> rdist(0, 200); // random allows even if the timing is off on the pi, that the random device has a more likely source of uniqueness
  msmtId = clientUid + "-" + getCurrentReadableTimestamp() + "_r" + std::to_string(rdist(rd));
}

std::string AutopowerClient::getSharedMsmtId() {
  std::unique_lock<std::mutex> fm(msmtIdMtx);
  return msmtId; // mutex unlocks automatically
}

uint32_t AutopowerClient::getInternalMsmtId() {
  // returns internal_measurement_id saved in the database table. Not linked with database
  return internalMmtId;
}

std::string AutopowerClient::readFileToString(const std::string &filename) { // source: https://github.com/grpc/grpc/issues/9593
  // read content of cert
  std::string data;
  std::ifstream file(filename.c_str(), std::ios::in);

  if (file.is_open()) {
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    data = ss.str();
  }

  return data;
}

struct CMsmtSample AutopowerClient::parseMsmt(std::string msmtLine) {
  CMsmtSample ms;
  std::string::size_type posOfComma = msmtLine.find(','); // find position of comma to split timestamp
  if (posOfComma == std::string::npos) {
    throw std::runtime_error("Could not find comma while parsing msmtLine. Got: " + msmtLine);
  }
  // parse and convert timestamp
  std::string timestamp = msmtLine.substr(0, posOfComma - 1);
  std::time_t msmttime = static_cast<time_t>(std::stod(timestamp)); // stod raises an exception if invalid input
  ms.timestamp = msmttime;
  // get milliseconds for high precission
  std::string::size_type posOfDotForMs = msmtLine.find('.');
  int32_t millis = 0; // milliseconds
  if (posOfDotForMs == std::string::npos) {
    std::cerr << "Warning: could not find dot for milliseconds in parsing line. Setting milliseconds to 0" << std::endl;
  } else {
    millis = std::stoul(msmtLine.substr(posOfDotForMs + 1, posOfComma - (posOfDotForMs + 1)));
  }
  ms.milliseconds = millis; // for high precission
  ms.rawTimestamp = timestamp;
  // parse and convert measurement data (unsigned long is at least int32_t)
  ms.measurement = std::stoul(msmtLine.substr(posOfComma + 1, std::string::npos));
  return ms;
}

std::unique_ptr<autopapi::CMeasurementApi::Stub> AutopowerClient::createGrpcConnection(std::string remoteHost, std::string remotePort, std::string privKeyClientPath, std::string pubKeyClientPath, std::string pubKeyCA) {
  std::shared_ptr<grpc::ChannelCredentials> cred;
  if (!privKeyClientPath.empty() && !pubKeyClientPath.empty()) {
    // setup encryption and authentication

    std::string privKey = readFileToString(privKeyClientPath);
    std::string pemChain = readFileToString(pubKeyClientPath);
    std::string caChain = "";
    if (!pubKeyCA.empty()) {
      std::cout << "Loading custom CA for connection to server..." << std::endl;
      caChain = readFileToString(pubKeyCA);
    }

    grpc::SslCredentialsOptions sslopt = {
        caChain, // caChain (CA) is by default lets encrypt, thus do not use the autopower CA if possible --> by default the CA string should be empty
        privKey,
        pemChain};

    cred = grpc::SslCredentials(sslopt);
  } else {
    std::cout << "WARNING: Server connection is unencrypted since private/public key path were not given. Use the -e argument to specify the Private key path and -f for the Public key path." << std::endl;
    cred = grpc::InsecureChannelCredentials();
  }

  // due to backoff in grpc we could have a slow re-connect. Thus have lower backoff like https://stackoverflow.com/a/77193007
  grpc::ChannelArguments chanOptions;
  chanOptions.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
  chanOptions.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 4000);
  std::shared_ptr<grpc::Channel> cnl = grpc::CreateCustomChannel(remoteHost + ":" + remotePort, cred, chanOptions);
  std::unique_ptr<autopapi::CMeasurementApi::Stub> stub = autopapi::CMeasurementApi::NewStub(cnl);
  return stub;
}

void AutopowerClient::notifyLED(std::string filepath, int waitTimes[], int numWaitElems, bool defaultOn) {
  // if running on raspi (4), we can control the onboard LEDs to convey information e.g. /sys/class/leds/PWR/brightness
  // the user running mmclient must have write access to this file
  std::ofstream ledStream(filepath);
  if (ledStream.is_open() && ledStream.good()) {
    if (defaultOn) {
      ledStream << "1"; // ensure that the LED is on at the start
    } else {
      ledStream << "0"; // ensure default off
    }
    ledStream.flush();
    bool sendOne = true;
    for (int i = 0; i < numWaitElems; i++) {
      // go through pattern
      if (sendOne) {
        ledStream << "1";
      } else {
        ledStream << "0";
      }

      sendOne = !sendOne;
      ledStream.flush();
      std::this_thread::sleep_for(std::chrono::milliseconds(waitTimes[i]));
    }

    // ensure on state of LED is as requested by defaultOn parameter
    if (defaultOn) {
      ledStream << "1";
    } else {
      ledStream << "0";
    }

    ledStream.flush();
  }

  ledStream.close();
}

void AutopowerClient::notifyPwrLED(int waitTimes[], int numWaitElems) {
  notifyLED("/sys/class/leds/PWR/brightness", waitTimes, numWaitElems, true);
}

void AutopowerClient::notifyActLED(int waitTimes[], int numWaitElems) {
  notifyLED("/sys/class/leds/ACT/brightness", waitTimes, numWaitElems, false);
}

void AutopowerClient::notifyLEDConnectionFailed() {
  int blinkPattern[6] = {150, 250, 150, 250, 300, 250};
  notifyPwrLED(blinkPattern, 6);
}

void AutopowerClient::notifyLEDSampleSaved() {
  int blinkPattern[4] = {25, 50,25,50};
  notifyActLED(blinkPattern,4);
}

void AutopowerClient::notifyLEDMeasurementCrashed() {
  int blinkPattern[8] = {250, 300,250,300, 250, 300, 300,400};
  notifyActLED(blinkPattern,8);
}

void AutopowerClient::getAndSavePpData() {
  // start pinpoint
  bool loggedErrorToServer = false; // only log starting errors to server once per measurement
  while (true) {
    // sleep this thread until we measure .
    std::shared_lock mm(measuringMtx);
    measuringCv.wait(mm, [this]() { return this->measuring(); });
    mm.unlock();

    // create pipe for communication
    int pipe_comm[2];
    if (pipe(pipe_comm) == -1) {
      std::string errMsg = "ERROR: Could not create pipe for communication with pinpoint!";
      std::cerr << errMsg << std::endl;
      putStatusToServer(1, errMsg);
      return;
    }

    // create pipe for error passing
    // int pipe_error_comm[2];
    // if (pipe(pipe_error_comm) == -1) {
    //  std::cerr << "ERROR: Could not create pipe for error communication with pinpoint!" << std::endl;
    //  return;
    // }

    setHasExited(false);
    // fork to execute pinpoint in seperate process
    pid_t pid = fork();

    if (pid == 0) {
      // child --> execute pinpoint
      close(pipe_comm[0]);
      // close(pipe_error_comm[0]);

      dup2(pipe_comm[1], STDOUT_FILENO); // ensure that stdout now goes to pipe
      // dup2(pipe_error_comm[1], STDERR_FILENO); // ensure that stderr now goes to pipe

      // std::cerr << "Starting pinpoint..." << std::endl;
      // pinpoint sometimes just terminates unexpectedly --> also happens on
      // CLI.
      try {
        std::stoi(ppSamplingInterval);
      } catch (std::invalid_argument const &sampInv) {
        std::string errMsg = "ERROR: Could not start pinpoint as sampling interval is invalid! Stopping measurement.";
        std::cerr << errMsg << std::endl;
        putStatusToServer(1, errMsg);
        stopMeasurement();
        return;
      }

      if (!isValidPpDevice(ppDevice)) {
        std::string errMsg = "ERROR: Could not start pinpoint as device is not whitelisted on the client and may not be supported! Please check the client side configuration! Stopping measurement.";
        std::cerr << errMsg << std::endl;
        putStatusToServer(1, errMsg);
        stopMeasurement();
        return;
      }

      if (!execlp(ppBinaryPath.c_str(), ppBinaryPath.c_str(), "--timestamp", "-c", "-e",
                  ppDevice.c_str(), "-i", ppSamplingInterval.c_str(), "-n", NULL)) {
        std::cerr << "ERROR: Could not start pinpoint!" << std::endl;
        putStatusToServer(1, "Could not start pinpoint. Please check the client for errors.");
      }
    } else {
      setCurrentlyRunningPid(pid); // save pid of pinpoint for being able to later on check in another thread
      // parent --> add data to DB
      close(pipe_comm[1]);
      // close(pipe_error_comm[1]);
      char rdbuffer;
      // char errrdbuffer;

      std::string line = "";

      // start connection to postgres for saving the data locally
      pqxx::connection pgcon{this->pgConString};
      // prepare insert of datapoints

      pgcon.prepare(
          "addMsmtPoint",
          "INSERT INTO measurement_data (internal_measurement_id, measurement_value, measurement_timestamp) VALUES ($1, $2, TO_TIMESTAMP($3))");
      // start transaction
      pqxx::work txn(pgcon);
      bool gotData = false; // log to output that we got data from pinpoint
      int ppWaitTime = 10;

      try { // calculate wait time based on sampling interval
        ppWaitTime += (std::stoll(ppSamplingInterval) / 1000);
      } catch (std::exception &e) {
        std::cerr << "Warning: Could not parse sampling interval: " << e.what() << ". Setting wait time for pinpoint failure detection to 10 seconds." << std::endl;
      }

      while (true) { // get next character with timeout
        if (!gotData) {
          gotData = true;
        }

        // set up polling and reading with timeout (see https://stackoverflow.com/a/56048171 as example). This restarts pinpoint if we didn't receive data after ppWaitTime e.g. on power loss of the power meter
        struct pollfd ppPollFd;
        ppPollFd.fd = pipe_comm[0];
        ppPollFd.events = POLLIN;

        int pollStatus = poll(&ppPollFd, 1, ppWaitTime * 1000); // check if new sample available
        if (pollStatus == -1) {
          std::string errorMsg = "Error: Could not poll on Pinpoint pipe. Killing pinpoint and trying again.";

          if (kill(pid, SIGQUIT) != 0) { // tell child to quit as we detected an error.
            std::cerr << "Could not kill pinpoint" << std::endl;
            errorMsg += " Also could not kill pinpoint. Please also check the client for hardware errors.";
          }

          std::cerr << errorMsg << std::endl;
          putStatusToServer(1, errorMsg);
          break;
        } else if (pollStatus == 0) {
          std::string errorMsg = "Error: Pinpoint didn't respond within a reasonable time with new data. Killing pinpoint and trying again.";

          if (kill(pid, SIGQUIT) != 0) { // tell child to terminate as we detected an error.
            errorMsg += "Also could not kill pinpoint. Please also check for hardware errors.";
          }

          std::cerr << errorMsg << std::endl;
          putStatusToServer(1, errorMsg);
          break;
        }

        int readChar = read(pipe_comm[0], &rdbuffer, 1); // read new character
        if (readChar <= 0) {
          // no new data
          break;
        }

        if (rdbuffer == '\n') {

          struct CMsmtSample msmtPoint = parseMsmt(line);
          try {
            txn.exec_prepared("addMsmtPoint", getInternalMsmtId(), msmtPoint.measurement, msmtPoint.rawTimestamp);
            txn.commit();
            setHasWrittenOnce(true); // specify success of writing at least once to DB
            setLastSampleTimestamp(msmtPoint.timestamp);
            // notify LED if possible
            notifyLEDSampleSaved();
          } catch (std::exception &e) {
            std::string exprContent = e.what();
            std::string errorMsg = "Error while writing measurement to database: " + exprContent;
            std::cerr << errorMsg << std::endl;

            // put error to server
            putStatusToServer(1, errorMsg);
          }
          line = "";
        } else {
          // add one non newline character to internal line buffer
          line.append(1, rdbuffer);
        }
        // check if we are still measuring
        if (!measuring()) {
          // std::cout << "getData(): Detected no longer measuring" <<
          // std::endl;
          break; // break out of loop to sleep
        }
      }

      close(pipe_comm[0]);

      if (!measuring()) {
        if (kill(pid, SIGTERM) != 0) { // tell child to terminate. This is the good case.
          std::cerr << "Could not kill pinpoint" << std::endl;
        }
      }

      int retStatus = 0;
      waitpid(pid, &retStatus, 0);
      setHasExited(true); // log the exit of pinpoint. Allows to get status
      if (retStatus != SIGTERM) {
        std::string errorMsg = "Pinpoint exited with non SIGTERM exit code " + std::to_string(retStatus) + ". Something may be wrong.";
        std::cerr << errorMsg << std::endl;
        if (!loggedErrorToServer) {
          putStatusToServer(1, errorMsg);
          loggedErrorToServer = true;
        }
      }
    }

    if (measuring()) {
      std::cerr << "Warning: Attempting to restart measurement since pinpoint exited..." << std::endl;
      notifyLEDMeasurementCrashed();
    } else {
      loggedErrorToServer = false; // restart to log errors for a new measurement
    }
  }
}

// Start a measurement
// returns pair with boolean telling success and string giving back shared measurment id of this measurement
std::pair<bool, std::string> AutopowerClient::startMeasurement() {
  std::unique_lock mm(measuringMtx);
  std::cout << "Starting new measurement..." << std::endl;
  // log measurement start to database
  try {
    pqxx::connection pgcon{this->pgConString};
    pgcon.prepare(
        "addMsmt",
        "INSERT INTO measurements (shared_measurement_id, client_uid) VALUES ($1, $2) RETURNING internal_measurement_id");
    setMsmtIdToNow();
    std::string sharedMsmtId = getSharedMsmtId();

    // save measurement start to database
    pqxx::work txn(pgcon);
    pqxx::result insRes = txn.exec_prepared("addMsmt", sharedMsmtId, clientUid);
    txn.commit();
    this->internalMmtId = insRes[0]["internal_measurement_id"].as<uint32_t>();
    // set up local measurement variables
    setHasWrittenOnce(false);
    setHasExited(true);
    setMeasuring(true);
    mm.unlock();
    measuringCv.notify_all();
    // wait until it is known that there was at least one write to the db. Then we assume that pinpoint can succeed.
    // this is not a fully sure method to check if the measurement will continue working as pinpoint may crash. For the actual current status, check the lastKnownPpPid.
    std::shared_lock wol(writtenOnceMtx);
    uint64_t waitTime = 10;
    try {
      waitTime += (std::stoll(ppSamplingInterval) / 1000);
    } catch (std::exception &e) {
      std::cerr << "Warning: Could not parse sampling interval: " << e.what() << ". Setting wait time to 10 seconds." << std::endl;
    }
    if (!hasWrittenOnceCv.wait_until(wol, std::chrono::system_clock::now() + std::chrono::seconds(waitTime), [this]() { return this->thisMeasurementHasWrittenOnce; })) {
      wol.unlock();
      std::cout << "Couldn't start measurement as the measurement did not write in the last " << waitTime << " seconds. Thus stopping again. Please check pinpoint output!" << std::endl;
      stopMeasurement();
      return std::pair<bool, std::string>(false, sharedMsmtId);
    }
    std::cout << "Started measurement." << std::endl;
    return std::pair<bool, std::string>(true, sharedMsmtId);
  } catch (std::exception &e) {
    std::cerr << "Error while starting measurement: " << e.what() << std::endl;
    return std::pair<bool, std::string>(false, "");
  }
}

bool AutopowerClient::stopMeasurement() {
  std::unique_lock mm(measuringMtx);
  bool stoppedMsmtSuccessfully = true;
  if (this->periodicUploadMinutes != 0) {
    // request upload of remaining data
    // the measuringCv will notify the thread later on
    this->setDoLastUpload(true);
  }

  setMeasuring(false);
  // we don't know if this measurement has exited yet. Thus not setting. Same goes for writtenOnce
  // log success of stopping measurement to server
  grpc::ClientContext stopMsmtCtxt;
  class autopapi::nothing nth;
  autopapi::cmMCode stopMsmtMsg;
  mm.unlock();
  measuringCv.notify_all(); // notify all that we are no longer measuring.

  // wait on exit status from measurement (= pinpoint has exited)
  std::shared_lock el(hasExitedMtx);
  hasExitedCv.wait(el, [this]() { return this->ppHasExited; });

  std::cout << "Stopped measurement." << std::endl;

  putStatusToServer(0, "Stopped measurement");
  return stoppedMsmtSuccessfully;
}

// uploads all names of measurements available on this device
bool AutopowerClient::uploadMeasurementList() {
  // setup sending stream
  grpc::ClientContext ufCtxt;
  class autopapi::nothing nth;
  // get available measurements from the DB
  pqxx::connection pgcon{this->pgConString};
  pqxx::work txn(pgcon);
  pqxx::result allMsmts = txn.exec("SELECT shared_measurement_id, internal_measurement_id FROM measurements");
  // write every measurement name to server
  std::unique_ptr<grpc::ClientWriter<autopapi::msmtName>> measurementNameStream(stub->putMeasurementList(&ufCtxt, &nth));

  for (auto const &tuple : allMsmts) {
    autopapi::msmtName msmt;
    msmt.set_clientuid(clientUid);
    msmt.set_name(tuple["internal_measurement_id"].as<std::string>() + " AKA " + tuple["shared_measurement_id"].as<std::string>());
    if (!measurementNameStream->Write(msmt)) {
      std::cerr << "Writing measurement names to server failed. Maybe the connetion failed?" << std::endl;
      break;
    }
    // docs (https://grpc.io/docs/languages/cpp/basics/) include a sleep here.
  }

  measurementNameStream->WritesDone(); // completed writes
  grpc::Status wStatus = measurementNameStream->Finish();
  if (!wStatus.ok()) {
    std::cerr << "Writing measurement names to server failed with error code " << wStatus.error_code() << ": " << wStatus.error_message() << std::endl
              << wStatus.error_details() << std::endl;
    return false;
  }

  return true;
}

// stream measurement with measId to server. If measID is empty, we transmit every not yet transmitted sample
// measId is default empty (""), see client.h
bool AutopowerClient::streamMeasurementData(std::string measId) {
  // stream content of measId content to server starting at line startlinenr. uint64_t should be long enough.

  // setup bool to tell successful streaming upload of all datapoints (this method may also throw an exception on failure)
  bool uploadWasSuccessful = true;
  // setup grpc stream
  grpc::ClientContext sFmCtxt;
  std::unique_ptr<grpc::ClientReaderWriter<autopapi::msmtSample, autopapi::sampleAck>> smpStream(stub->putMeasurement(&sFmCtxt));

  std::thread streamWriterThread([&] {
    // upload measurement samples
    pqxx::connection pgReadCon{this->pgConString};
    pqxx::work readTxn(pgReadCon);
    std::string getMmStartSql = "SELECT md_id, measurement_value, measurement_timestamp, measurements.internal_measurement_id AS internal_measurement_id, measurements.shared_measurement_id AS shared_mm_id, measurements.client_uid FROM measurement_data, measurements WHERE measurements.internal_measurement_id = measurement_data.internal_measurement_id";

    if (measId.empty()) {
      getMmStartSql += " AND was_uploaded = false";
    } else {
      // MUST escape via readTxn.esc since not found how to give parameters to cursor.
      getMmStartSql += " AND measurements.shared_measurement_id = '" + readTxn.esc(measId) + "'";
    }

    // https://stackoverflow.com/questions/16128142/how-to-use-pqxxstateless-cursor-class-from-libpqxx

    pqxx::stateless_cursor<pqxx::cursor_base::read_only, pqxx::cursor_base::owned> tuplesToStream(readTxn, getMmStartSql, "uploadTupleCurs", false); // the tuples returned from the database to be uploaded

    int numTuplesNotWritten = 0;
    for (size_t idx = 0; true; idx++) {
      pqxx::result res = tuplesToStream.retrieve(idx, idx + 1);
      if (res.empty()) {
        // on cursor end --> exit
        break;
      }

      auto tuple = res[0];
      // build up the measurement sample for grpc based on the DB content
      autopapi::msmtSample grpcMsmtSample;
      if (tuple["client_uid"].as<std::string>() != clientUid) {
        std::cout << "Warning: Uploading measurement from different clientUid then currently set." << std::endl;
      }
      grpcMsmtSample.set_clientuid(tuple["client_uid"].as<std::string>());
      grpcMsmtSample.set_msmtcontent(tuple["measurement_value"].as<uint32_t>());
      grpcMsmtSample.set_msmtid(tuple["shared_mm_id"].as<std::string>());
      grpcMsmtSample.set_sampleid(tuple["md_id"].as<uint64_t>());
      // we must create a new object/timestamp ptr to pass it to the sample. It will be automatically freed by grpc
      google::protobuf::Timestamp *gTimestamp = new google::protobuf::Timestamp();
      std::string ts = tuple["measurement_timestamp"].as<std::string>();
      std::replace(ts.begin(), ts.end(), ' ', 'T');
      bool couldParseTs = google::protobuf::util::TimeUtil::FromString(ts, gTimestamp);

      if (!couldParseTs) {
        delete gTimestamp;
        uploadWasSuccessful = false;
        throw std::runtime_error("Could not parse timestamp: " + ts);
      }

      grpcMsmtSample.set_allocated_msmttime(gTimestamp); // grpc will free the object itself, so NO need to call free!

      // finally write to server
      bool couldWrite = smpStream->Write(grpcMsmtSample);
      if (!couldWrite) {
        numTuplesNotWritten++;
        // exit early
        break;
      }
    }

    if (numTuplesNotWritten > 0) {
      uploadWasSuccessful = false;
      std::cerr << "Could not write tuples to the server. Maybe the connection failed?" << std::endl;
    }

    smpStream->WritesDone(); // completed writes
  });

  std::thread streamAckThread([&] {
    // collect the ACKs for uploaded measurements
    pqxx::connection pgcon{this->pgConString};
    pqxx::work txn(pgcon);
    pgcon.prepare( // sets all correctly uploaded datapoints to be uploaded
        "confirmMmUploaded",
        "UPDATE measurement_data SET was_uploaded = true WHERE md_id = $1");
    grpc::ClientContext cc;
    autopapi::sampleAck smpAck;
    while (smpStream->Read(&smpAck)) {
      txn.exec_prepared("confirmMmUploaded", smpAck.sampleid());
    }
    txn.commit();
  });

  streamWriterThread.join();
  streamAckThread.join();

  grpc::Status wStatus = smpStream->Finish();
  if (!wStatus.ok()) {
    uploadWasSuccessful = false;
    std::string errorMsg = "Writing samples to server failed: " + wStatus.error_message() + "; " + wStatus.error_details();
    // write failed. Since we only set the was_uploaded field to true if Write() returned true, we know the server at least received the tuples.
    // thus no need to revert the transaction.
    std::cerr << errorMsg << std::endl;
    sFmCtxt.TryCancel(); // cancel this context
    throw std::runtime_error(errorMsg);
  }

  return uploadWasSuccessful;
}

void AutopowerClient::doPeriodicDataUpload() {
  // calls uploadFinishedMeasurement every periodicUploadMinutes minutes to upload the data of the measurement and creates a new file - if there is a running measurement

  while (true) {
    // ensure we are actually measuring
    std::shared_lock mm(measuringMtx);
    // only continue if we actually want to upload and are measuring or requested a final upload (requested in stopMeasurement())
    measuringCv.wait(mm, [this]() { return (this->measuring() || this->getDoLastUpload()); });
    // check if we actually want to transmit
    if (this->periodicUploadMinutes == 0 && !this->getDoLastUpload()) {
      measuringCv.wait(mm, [this]() { return !this->measuring(); });
      // wait until no longer measuring
      continue; // since the periodic upload minutes is set to zero, by definition we do not transmit --> wait on next wake up of measurement. Will also wake up if the measurement is set to finish, but this doesn't matter
    }
    // actually transmit the data to the server
    try {
      if (!streamMeasurementData()) {
        throw std::runtime_error("Data upload failed!");
      }
    } catch (std::exception &e) {
      std::string excContent = e.what();
      std::cerr << "Error: " << excContent << std::endl;
      // also try to log error on server
      putStatusToServer(1, "Data upload got error: " + excContent);
    }

    // Reset DoLastUpload since we uploaded successfully.
    this->setDoLastUpload(false);
    measuringCv.wait_for(mm, std::chrono::minutes(this->periodicUploadMinutes));
    mm.unlock();
  }

  std::cerr << "Error: Periodic upload thread exited." << std::endl;
  putStatusToServer(1, "Error: Periodic upload thread exited. This points to a crash.");
}

// functions to handle requests from the server

// Server wants to start a measurement
void AutopowerClient::handleMeasurementStart(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  if (measuring()) {
    std::string errorDescription = "Warning: Received START_MEASUREMENT, even though already measuring. Ignoring request.";
    std::cerr << errorDescription << std::endl;
    // put warning to server
    putResponseToServer(1, errorDescription, autopapi::clientResponseType::STARTED_MEASUREMENT_RESPONSE, sRequest.requestno());
    return;
  }

  autopapi::msmtSettings mset;
  grpc::ClientContext mCtxt;

  stub->getMsmtSttngsAndStart(&mCtxt, cluid, &mset);
  if (mset.clientuid() != cluid.uid()) {
    std::string errorDescription = "Warning: Received measurement start for wrong client. Ignoring.";
    std::cerr << errorDescription << std::endl;

    // put warning to server
    putResponseToServer(1, errorDescription, autopapi::clientResponseType::STARTED_MEASUREMENT_RESPONSE, sRequest.requestno());
    return;
  }

  // verify data
  std::string newPpdev = mset.ppdevice();
  if (!isValidPpDevice(newPpdev)) {
    std::string errorDescription = "Error: Received invalid device for pinpoint. Only ";

    for (std::string dev : this->supportedDevices) {
      errorDescription += dev + ", ";
    }

    // fencepost solving:
    errorDescription.pop_back();
    errorDescription.pop_back();

    errorDescription += " allowed. Ignoring request.";
    std::cerr << errorDescription << std::endl;

    // put warning to server
    putResponseToServer(1, errorDescription, autopapi::clientResponseType::STARTED_MEASUREMENT_RESPONSE, sRequest.requestno());
    return;
  }

  std::string newSamplingInt = mset.ppsamplinginterval();
  if (newSamplingInt == "") { // TODO: Add more validity checking
    std::string errorDescription = "Sampling interval has invalid content. Not starting measurement.";
    std::cerr << errorDescription << std::endl;

    // put warning to server
    putResponseToServer(1, errorDescription, autopapi::clientResponseType::STARTED_MEASUREMENT_RESPONSE, sRequest.requestno());
    return;
  }
  uint32_t uploadIntMin = mset.uploadintervalmin(); // the interval to upload the content to the server
  // Set data. ensure that we have measurement access --> lock the measurement and then data lock. ATTENTION: ensure correct ordering for deadlock prevention
  std::unique_lock mm(measuringMtx);
  ppSamplingInterval = newSamplingInt.c_str();
  ppDevice = newPpdev.c_str();
  this->periodicUploadMinutes = uploadIntMin;
  mm.unlock();

  // as the data is set, we can now start the measurement

  std::pair startMsmtStatus = startMeasurement();

  // log success/failure of starting measurement to server

  uint32_t statusCode = 0;
  std::string statusMsg = startMsmtStatus.second;
  if (!startMsmtStatus.first) {
    // measurement start failed
    statusCode = 1;
    statusMsg = "Could not start measurement successfully. Please check the client for error messages from pinpoint.";
  }

  putResponseToServer(statusCode, statusMsg, autopapi::clientResponseType::STARTED_MEASUREMENT_RESPONSE, sRequest.requestno());
}

// Server wants to stop a measurement
void AutopowerClient::handleMeasurementStop(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  uint32_t statusCode = 0;
  std::string statusMsg = "Measurement stopped successfully";
  if (!stopMeasurement()) {
    statusCode = 1;
    statusMsg = "Measurement didn't stop successfully. Please check for errors on the client.";
  }

  putResponseToServer(statusCode, statusMsg, autopapi::clientResponseType::STOPPED_MEASUREMENT_RESPONSE, sRequest.requestno());
}

// Server wants to check if client is alive
void AutopowerClient::handleIntroduceServer(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  // put pong response to server
  putResponseToServer(0, "PONG", autopapi::clientResponseType::INTRODUCE_CLIENT, sRequest.requestno());
}

// server requests list of measurements in clients DB
void AutopowerClient::handleMeasurementList(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  uint32_t statusCode = 0;
  std::string statusMsg = "Sent list successfully";
  if (!uploadMeasurementList()) {
    statusCode = 1;
    statusMsg = "Could not send measurement list. Please check client for errors.";
  }

  putResponseToServer(statusCode, statusMsg, autopapi::clientResponseType::MEASUREMENT_LIST_RESPONSE, sRequest.requestno());
}

// server requests measurement status of client
void AutopowerClient::handleMeasurementStatus(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  Json::Value statusObject;
  statusObject["inMeasuringMode"] = measuring(); // say if we are currently in measuring mode. Doesn't mean pinpoint runs
  statusObject["ppHasWrittenOnce"] = getHasWrittenOnce();
  statusObject["ppHasExited"] = getHasExited();
  // get data related to current measurement
  std::shared_lock msmtLock(measuringMtx);
  statusObject["measurementSettings"]["ppDevice"] = this->ppDevice;
  statusObject["measurementSettings"]["ppSamplingInterval"] = this->ppSamplingInterval;
  statusObject["measurementSettings"]["uploadInterval"] = periodicUploadMinutes;
  statusObject["measurementSettings"]["sharedMsmtId"] = getSharedMsmtId();
  statusObject["ppIsRunning"] = false;
  statusObject["lastSampleTimestamp"] = getReadableTimestamp(getLastSampleTimestamp());
  if (measuring() && getHasWrittenOnce() && !getHasExited()) {
    // we assume that pinpoint should now be running. Check with kill() via getPpIsCurrentlyRunning()
    statusObject["ppIsRunning"] = getPpIsCurrentlyRunning();
  }
  msmtLock.unlock();
  Json::StreamWriterBuilder builder;
  builder["indentation"] = ""; // to save space, do not have any indentation
  std::string statusString = Json::writeString(builder, statusObject);
  putResponseToServer(0, statusString, autopapi::clientResponseType::MEASUREMENT_STATUS_RESPONSE, sRequest.requestno());
}

// server requests data for some measurement from client
void AutopowerClient::handleMeasurementData(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  uint32_t statusCode = 0;
  std::string statusMsg = "";
  try {
    // TODO: Maybe make this async with a condition variable
    if (!streamMeasurementData(sRequest.requestbody())) {
      throw std::runtime_error("streamMeasurementData raised an exception while requesting measurement data.");
    }
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    statusCode = 1;
    statusMsg = e.what();
  }

  putResponseToServer(statusCode, statusMsg, autopapi::clientResponseType::MEASUREMENT_DATA_RESPONSE, sRequest.requestno());
}

// handle get request from server and issue commands
void AutopowerClient::handleSrvRequest(autopapi::srvRequest sRequest, autopapi::clientUid cluid) {
  if (sRequest.msgtype() == autopapi::srvRequestType::START_MEASUREMENT) {
    std::cout << "Received START_MEASUREMENT" << std::endl;
    handleMeasurementStart(sRequest, cluid);
  } else if (sRequest.msgtype() == autopapi::srvRequestType::STOP_MEASUREMENT) {
    std::cout << "Received STOP_MEASUREMENT" << std::endl;
    handleMeasurementStop(sRequest, cluid);
  } else if (sRequest.msgtype() == autopapi::srvRequestType::INTRODUCE_SERVER) {
    std::cout << "Received INTRODUCE_SERVER" << std::endl;
    handleIntroduceServer(sRequest, cluid);
  } else if (sRequest.msgtype() == autopapi::srvRequestType::REQUEST_MEASUREMENT_LIST) {
    // server requested us to upload the current file list
    std::cout << "Received REQUEST_MEASUREMENT_LIST" << std::endl;
    handleMeasurementList(sRequest, cluid);
  } else if (sRequest.msgtype() == autopapi::srvRequestType::REQUEST_MEASUREMENT_STATUS) {
    std::cout << "Received REQUEST_MEASUREMENT_STATUS" << std::endl;
    handleMeasurementStatus(sRequest, cluid);
  } else if (sRequest.msgtype() == autopapi::srvRequestType::REQUEST_MEASUREMENT_DATA) {
    std::cout << "Received REQUEST_MEASUREMENT_DATA" << std::endl;
    handleMeasurementData(sRequest, cluid);
  } else {
    std::cerr << "Received unknown message type: " << sRequest.msgtype() << std::endl;
  }
}

void AutopowerClient::manageMsmt() {
  // monitor thread creates the upload thread to do the periodic upload.
  while (true) {
    // set uid
    class autopapi::clientUid cluid;
    cluid.set_uid(clientUid);
    // set void/autopapi::nothing return object
    class autopapi::nothing n;

    // initialize context
    grpc::ClientContext cc;
    autopapi::srvRequest sRequest;
    std::cout << "Registering at server..." << std::endl;
    std::unique_ptr<grpc::ClientReader<autopapi::srvRequest>> serverApiStream(stub->registerClient(&cc, cluid));

    // create a executor to allow multiple concurrently running sRequests
    // follows boosts thread_pool as described in https://stackoverflow.com/a/54436167
    boost::asio::thread_pool srvRequestPool(4); // only allow 4 concurrent requests. There should not be too many anyway
    while (serverApiStream->Read(&sRequest)) {
      boost::asio::post(srvRequestPool, [this, sRequest, cluid] {
        handleSrvRequest(sRequest, cluid);
      });
      // wait for requests from server.
      // handleSrvRequest(sRequest, cluid);
    }

    srvRequestPool.join();

    grpc::Status sf = serverApiStream->Finish();
    std::cerr << sf.error_code() << ": " << sf.error_message() << ": " << sf.error_details() << std::endl;
    std::cerr << "Server stream exited. Will attempt re-register." << std::endl;
    cc.TryCancel(); // try to free up context as this is no longer possible to use.
    notifyLEDConnectionFailed();
    sleep(5); // sleep for 5 seconds before re-registering
  }

  std::cerr << "manageMsmt() exited..." << std::endl;
  putStatusToServer(1, "manageMsmt() thread exited. This points to a crash.");
}

AutopowerClient::AutopowerClient(std::string _clientUid,
                                 std::string _remoteHost, std::string _remotePort, std::string _privKeyPath, std::string _pubKeyPath, std::string _pubKeyCA,
                                 std::string _pgConnString,
                                 std::string _ppBinaryPath, std::string _ppDevice, std::string _ppSamplingInterval, std::vector<std::string> _supportedDevices)
    : // set up variables for environment
      clientUid(_clientUid),
      pgConString(_pgConnString),
      ppBinaryPath(_ppBinaryPath), ppDevice(_ppDevice),
      ppSamplingInterval(_ppSamplingInterval), supportedDevices(_supportedDevices) {
  // connect to external server
  this->stub = createGrpcConnection(_remoteHost, _remotePort, _privKeyPath, _pubKeyPath, _pubKeyCA);
  std::cout << "Started client UID " << clientUid << " and ppBinaryPath: " << ppBinaryPath << std::endl;
  // start client running in multiple threads
  std::thread managementThread(&AutopowerClient::manageMsmt, this); // thread to connect to server and for API
  std::thread measurementThread(&AutopowerClient::getAndSavePpData, this);
  std::thread uploadThread(&AutopowerClient::doPeriodicDataUpload, this);
  startMeasurement();
  uploadThread.join(); // should never get here
  measurementThread.join();
  managementThread.join();
}

int main(int argc, char **argv) {
  std::string clientUid = "";                // unique ID for this client
  std::string remoteHost = "";               // domain of control server
  std::string remotePort = "";               // port of server
  std::string privKeyPath = "";              // path to private key for authentification to server
  std::string pubKeyPath = "";               // path to public key for authentification to server
  std::string pubKeyCA = "";                 // path to public key of custom CA. Only use this if the servers CA is not trusted
  std::string ppBinaryPath = "";             // absolute path to pinpoint binary
  std::string ppDevice = "";                 // device to measure (MCP1, MCP2, CPU etc.)
  std::string ppSamplingInterval = "";       // sampling interval for pinpoint in ms
  std::string secretsFilePath = "";          // absolute path to secrets (postgres string, certs, ...)
  std::string configFilePath = "";           // absolute path to config file (JSON)
  std::string postgresString = "";           // string to connect to postgres DB
  std::vector<std::string> supportedDevices; // vector of allowed and supported devices for pinpoint

  // get arguments from cli
  int arg;
  while ((arg = getopt(argc, argv, "u:r:p:b:d:i:e:f:s:c:h")) != -1) {
    switch (arg) {
    case 'u': // client uid
      clientUid = optarg;
      break;
    case 'r': // remote host
      remoteHost = optarg;
      break;
    case 'p': // remote port
      remotePort = optarg;
      break;
    case 'e': // path to private key
      privKeyPath = optarg;
      break;
    case 'f': // path to public key
      pubKeyPath = optarg;
      break;
    case 'b': // path to pinpoint binary
      ppBinaryPath = optarg;
      break;
    case 'd': // device
      ppDevice = optarg;
      break;
    case 'i': // sampling interval
      ppSamplingInterval = optarg;
      break;
    case 's': // secrets file path for passwords etc.
      secretsFilePath = optarg;
      break;
    case 'c': // path to config file. CLI args overwrite config file contents
      configFilePath = optarg;
      break;
    case 'h': // print help
      std::cerr << "Autopower measurement client. Available flags:" << std::endl
                << "  Use at least the -s flag to specify a secrets file." << std::endl
                << std::endl
                << "  -u An unique client uid" << std::endl
                << "  -k Output key. A string for the shared measurement uid. Can be random." << std::endl
                << "  -r Remote host. The IP or domain of the remote control server." << std::endl
                << "  -p Remote port. Port of the remote control server." << std::endl
                << "  -e path to private key" << std::endl
                << "  -f path to public key" << std::endl
                << "  -b path to pinpoint binary" << std::endl
                << "  -d standard device to measure via pinpoint" << std::endl
                << "  -i standard sampling interval for pinpoint" << std::endl
                << "  -s path to secrets config file containing paths to the certificates" << std::endl
                << "  -c path to config file" << std::endl
                << "  -h print this help information" << std::endl;
      return 0;

    default:
      std::cerr << "Available flages: u,k,r,p,b,d,i,s,c,h. Please see -h for explanation" << std::endl;
      return -1;
    }
  }

  // read secrets from config file
  if (secretsFilePath.empty()) {
    std::cerr << "No secrets file given. Please use the -s flag to specify a file!" << std::endl;
    return -1;
  }

  std::ifstream secretsFile(secretsFilePath, std::ifstream::binary);
  if (!secretsFile) {
    std::cerr << "Could not open secrets file. Please check if the file exists and permissons." << std::endl;
    return -1;
  }

  Json::Value secrets;
  secretsFile >> secrets;
  if (!secrets["postgresString"]) { // database connection is required to stand
    std::cerr << "Secrets file does not contain postgresString for connecting to database. Please check the file format!" << std::endl;
    return -1;
  } else {
    postgresString = secrets["postgresString"].asString();
  }

  if (secrets["privKeyPath"] && secrets["pubKeyPath"] && privKeyPath.empty() && pubKeyPath.empty()) {
    // set privKeyPath and pubKey path only if set in the json file and they weren't overwritten by the cli args
    privKeyPath = secrets["privKeyPath"].asString();
    pubKeyPath = secrets["pubKeyPath"].asString();
    if (!secrets["pubKeyCA"].empty()) {
      // only use a non trusted, custom CA if set
      pubKeyCA = secrets["pubKeyCA"].asString();
    }
  }

  // fill other arguments from config file if not set via CLI
  if (!configFilePath.empty()) {
    std::ifstream configFile(configFilePath, std::ifstream::binary);
    if (!configFile) {
      std::cerr << "Could not open config file. Please check if the file exists and permissons." << std::endl;
      return -1;
    }

    Json::Value config;
    configFile >> config;

    if (clientUid.empty() && config["clientUid"]) {
      clientUid = config["clientUid"].asString();
    }

    if (remoteHost.empty() && config["remoteHost"]) {
      remoteHost = config["remoteHost"].asString();
    }

    if (remotePort.empty() && config["remotePort"]) {
      remotePort = config["remotePort"].asString();
    }

    if (ppBinaryPath.empty() && config["ppBinaryPath"]) {
      ppBinaryPath = config["ppBinaryPath"].asString();
    }

    if (ppDevice.empty() && config["ppDevice"]) {
      ppDevice = config["ppDevice"].asString();
    }

    if (ppSamplingInterval.empty() && config["ppSamplingInterval"]) {
      ppSamplingInterval = config["ppSamplingInterval"].asString();
    }

    if (!config["supportedDevices"]) {
      std::cerr << "Could not find any supported devices in config file. Please whitelist the available counters you get via running pinpoint -l!" << std::endl;
      return -1;
    } else {
      // we can get the devices --> save as allowed ones.
      const Json::Value declaredDevices = config["supportedDevices"];
      for (int i = 0; i < declaredDevices.size(); i++) {
        supportedDevices.push_back(declaredDevices[i].asString());
      }
    }
  }

  if (clientUid.empty()) {
    std::random_device rd;
    std::uniform_int_distribution<int> rdist(200, 2000);
    clientUid = "autopower" + std::to_string(rdist(rd));
  }

  if (remoteHost.empty()) {
    remoteHost = "localhost";
  }

  if (remotePort.empty()) {
    remotePort = "25181";
  }

  if (ppBinaryPath.empty()) {
    ppBinaryPath = "/usr/bin/pinpoint";
  }

  if (ppDevice.empty()) {
    ppDevice = "MCP1";
  }

  if (ppSamplingInterval.empty()) {
    ppSamplingInterval = "500";
  }

  // finally start client
  try {
    AutopowerClient c(clientUid, remoteHost, remotePort, privKeyPath, pubKeyPath, pubKeyCA, postgresString, ppBinaryPath, ppDevice, ppSamplingInterval, supportedDevices);
  } catch (std::exception &e) {
    std::cerr << "FATAL ERROR: An exception occurred: " << e.what() << std::endl;
  }
}
