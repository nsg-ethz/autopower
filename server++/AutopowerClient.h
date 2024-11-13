#pragma once

#include "api.pb.h"
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <string>
#include <unordered_map>

// Represents an autopower device
class AutopowerClient {
public:
    AutopowerClient(const std::string& uid);

    void setPpDevice(const std::string& ppdev);
    std::string getPpDevice() const;

    void setPpSampleInterval(const std::string& sampInt);
    std::string getPpSampleInterval() const;

    void setUploadIntervalMin(int uploadInt);
    int getUploadIntervalMin() const;

    void scheduleDeletion();
    std::unordered_map<std::string, std::string> getMsmtSettings() const;

    int scheduleRequest(autopapi::srvRequest* job);
    autopapi::srvRequest* getNextRequest();
    void setResponse(autopapi::clientResponse response);

    bool responseArrived(int requestNo) const;
    autopapi::clientResponse getResponse(int requestNo) const;
    bool waitForResponseTo(int requestNo);
    void purgeRequestNo(int requestNo);

private:
    std::string uid;
    std::queue<autopapi::srvRequest*> jobqueue; // TODO: Maybe think about Lifo queue
    mutable std::mutex jobQueueMutex;
    std::condition_variable jobQueueCondition;

    std::map<int, autopapi::clientResponse> responses;
    mutable std::mutex responseslock;
    std::condition_variable responsescv;
    std::mutex seqnoLock;
    int lastRequestNo;

    // Standard measurement settings
    std::string ppDevice;
    std::string ppSamplingInterval;
    int uploadIntervalMin;
};
