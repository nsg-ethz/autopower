#pragma once
#include "api.grpc.pb.h"
#include "api.pb.h"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <condition_variable>
#include <csignal>
#include <ctime>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

// describes a measurement sample
struct CMsmtSample {
  time_t timestamp;
  std::string rawTimestamp; // raw timestamp (epoch)
  int32_t milliseconds;     // for sub second precission allow milliseconds
  uint32_t measurement;
};

class AutopowerClient {
  std::string clientUid;
  std::string ppBinaryPath;                  // absolute path to pinpoint binary
  std::string ppDevice;                      // device to run pinpoint on
  std::string ppSamplingInterval;            // sampling interval for pinpoint
  std::string pgConString;                   // string for connecting to postgres DB
  std::string msmtId;                        // shared id of this measurement
  std::vector<std::string> supportedDevices; // supported devices by pinpoint during last time pinpoint was queried
  uint32_t internalMmtId;                    // local (private to database) ID of this measurement
  std::mutex msmtIdMtx;
  std::shared_mutex hasExitedMtx;          // mutex to protect the exit/running status of pinpoint
  std::condition_variable_any hasExitedCv; // Condition variable to notify about termination of pinpoint

  bool ppHasExited = true;
  bool isMeasuring = false;
  bool doLastUpload = false; // boolean to allow forcing one upload via upload thread

  std::shared_mutex measuringMtx;
  std::condition_variable_any measuringCv;

  std::shared_mutex writtenOnceMtx;             // lock to protect the written once variable
  std::condition_variable_any hasWrittenOnceCv; // condition variable to notify successful write once
  std::shared_mutex ppIsRunningMtx;
  pid_t lastKnownPpPid; // the last known pid from pinpoint. Used to check if pinpoint is actually running in status message
  bool thisMeasurementHasWrittenOnce = false;
  uint32_t periodicUploadMinutes = 5; // time to periodically upload the data in minutes. If this is 0 we never upload the content and rely on manually uploading the files

  std::time_t lastSampleTimestamp; // the last timestamp in raw string form written to the DB
  std::shared_mutex lastSampleMtx; // sample mutex

  void setLastSampleTimestamp(std::time_t lastSampleTimestamp) {
    std::unique_lock<std::shared_mutex> wl(lastSampleMtx);
    this->lastSampleTimestamp = lastSampleTimestamp;
  }

  std::time_t getLastSampleTimestamp() {
    std::shared_lock<std::shared_mutex> rl(lastSampleMtx);
    return this->lastSampleTimestamp;
  }

  std::unique_ptr<autopapi::CMeasurementApi::Stub> stub; // stub for connecting to GRPC server
  bool measuring() {
    // read measuring
    std::shared_lock mm(measuringMtx);
    return this->isMeasuring;
  }

  void setMeasuring(bool measure) {
    // write measuring
    this->isMeasuring = measure;
  }

  bool getHasWrittenOnce() {
    std::shared_lock mm(writtenOnceMtx);
    return this->thisMeasurementHasWrittenOnce;
  }

  void setHasWrittenOnce(bool writtenOnce) {
    std::unique_lock<std::shared_mutex> wl(writtenOnceMtx);
    this->thisMeasurementHasWrittenOnce = writtenOnce;
    if (writtenOnce) {
      hasWrittenOnceCv.notify_all();
    }
  }

  bool getHasExited() {
    std::shared_lock el(hasExitedMtx);
    return this->ppHasExited;
  }

  void setHasExited(bool exited) {
    std::unique_lock<std::shared_mutex> el(hasExitedMtx);
    this->ppHasExited = exited;
    el.unlock();
    if (exited) {
      hasExitedCv.notify_all();
    }
  }

  void setDoLastUpload(bool doUpload) {
    this->doLastUpload = doUpload;
  }

  bool getDoLastUpload() {
    return this->doLastUpload;
  }

  void setCurrentlyRunningPid(pid_t pid) {
    std::unique_lock<std::shared_mutex> rlck(ppIsRunningMtx);
    this->lastKnownPpPid = pid;
  }

  bool getPpIsCurrentlyRunning() {
    std::shared_lock rlck(ppIsRunningMtx);
    return 0 == kill(lastKnownPpPid, 0);
  }

  void updateValidPpDeviceList();
  bool isValidPpDevice(std::string device);

  void putStatusToServer(uint32_t statuscode, std::string message);
  void putResponseToServer(uint32_t statuscode, std::string message, autopapi::clientResponseType respType, uint32_t requestNo);

  std::string getReadableTimestamp(std::time_t rtme);
  std::string getCurrentReadableTimestamp();
  void setMsmtIdToNow();
  std::string getSharedMsmtId();
  uint32_t getInternalMsmtId();
  std::string readFileToString(const std::string &filename); // TODO: remove this method from the client as it's an util function
  struct CMsmtSample parseMsmt(std::string msmtLine);
  std::unique_ptr<autopapi::CMeasurementApi::Stub> createGrpcConnection(std::string remoteHost, std::string remotePort, std::string privKeyClientPath, std::string pubKeyClientPath, std::string pubKeyCA = "");

  void notifyLED(std::string filePath, int waitTimes[], int numWaitElems, bool defaultOn);
  void notifyPwrLED(int waitTimes[], int numWaitElems);
  void notifyActLED(int waitTimes[], int numWaitElems);
  void notifyLEDConnectionFailed();
  void notifyLEDSampleSaved();

  void notifyLEDMeasurementCrashed();

  void getAndSavePpData();
  std::pair<bool, std::string> startMeasurement();
  bool stopMeasurement();
  bool uploadMeasurementList();
  bool streamMeasurementData(std::string measId = "");
  void doPeriodicDataUpload();

  void handleMeasurementStart(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleMeasurementStop(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleIntroduceServer(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleMeasurementList(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleMeasurementStatus(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleMeasurementData(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void handleAvailablePPDevice(autopapi::srvRequest sRequest, autopapi::clientUid cludi);

  void handleSrvRequest(autopapi::srvRequest sRequest, autopapi::clientUid cluid);
  void manageMsmt();

public:
  AutopowerClient(std::string _clientUid,
                  std::string _remoteHost, std::string _remotePort, std::string _privKeyPath, std::string _pubKeyPath, std::string _pubKeyCA,
                  std::string _pgConnString,
                  std::string _ppBinaryPath, std::string _ppDevice, std::string _ppSamplingInterval);
};