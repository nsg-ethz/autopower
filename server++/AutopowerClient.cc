#include <cstddef>
#include <map>
#include <stdexcept>
#include "AutopowerClient.h"
#include "api.pb.h"

AutopowerClient::AutopowerClient(const std::string& uid)
    : uid(uid), lastRequestNo(0), ppDevice("MCP1"), ppSamplingInterval("500"), uploadIntervalMin(5) {}

// setter and getter for AutopowerClient methods
void AutopowerClient::setPpDevice(const std::string& ppdev) {
    this->ppDevice = ppdev;
}

std::string AutopowerClient::getPpDevice() const {
    return this->ppDevice;
}

void AutopowerClient::setPpSampleInterval(const std::string& sampInt) {
    // TODO: make this integer.
    this->ppSamplingInterval = sampInt;
}

std::string AutopowerClient::getPpSampleInterval() const {
    return this->ppSamplingInterval;
}

void AutopowerClient::setUploadIntervalMin(int uploadInt) {
    this->uploadIntervalMin = uploadInt;
}

int AutopowerClient::getUploadIntervalMin() const {
    return this->uploadIntervalMin;
}

void AutopowerClient::scheduleDeletion() {
    // TODO: This may not be necessary. Refactor this!
    std::queue<autopapi::srvRequest> empty;
    std::swap(this->jobqueue, empty);
}

std::unordered_map<std::string, std::string> AutopowerClient::getMsmtSettings() const {
    // TODO: may need to rewrite this into JSON
    return {
        {"ppDevice", ppDevice},
        {"ppSamplingInterval", ppSamplingInterval},
        {"uploadIntervalMin", std::to_string(uploadIntervalMin)}
    };
}

// Schedule a new request to this client.
// Adds a job to the job queue with an incremented sequence/request number.
int AutopowerClient::scheduleRequest(autopapi::srvRequest job) {
    std::lock_guard<std::mutex> lock(seqnoLock);
    lastRequestNo++;
    job.set_requestno(lastRequestNo);
    jobqueue.push(job);
    jobQueueCondition.notify_all();
    return lastRequestNo;
}

// Retrieves the next job from the queue, blocking if empty.
autopapi::srvRequest AutopowerClient::getNextRequest() {
    std::unique_lock<std::mutex> lock(jobQueueMutex);
    jobQueueCondition.wait(lock, [this]() { return !jobqueue.empty(); });
    autopapi::srvRequest nextJob = jobqueue.front();
    jobqueue.pop(); // remove this request
    return nextJob;
}

// Save response in local map and notifies all waiting threads.
void AutopowerClient::setResponse(autopapi::clientResponse response) {
    std::lock_guard<std::mutex> lock(responseslock);
    responses[response.requestno()] = response;
    responsescv.notify_all();
}

// Checks if the response for a specific request number has arrived.
bool AutopowerClient::responseArrived(int requestNo) const {
    std::lock_guard<std::mutex> lock(responseslock);
    // we assume that if requestNo in responses, that this is valid (and never NULL)
    return responses.find(requestNo) != responses.end();
}

// Retrieves the response for a given request number if it has arrived, otherwise throws an error.
autopapi::clientResponse AutopowerClient::getResponse(int requestNo) const {
    std::lock_guard<std::mutex> lock(responseslock);
    if (!responseArrived(requestNo)) {
        throw std::invalid_argument("Cannot call getResponse() on non-arrived requestNo " + std::to_string(requestNo));
    }
    return responses.at(requestNo);
}

// Waits until response of requestNo arrived or timeout occurred
bool AutopowerClient::waitForResponseTo(int requestNo) {
    // TODO: Validate semantics here
    std::unique_lock<std::mutex> lock(responseslock);
    // wait until response arrived or timeout occurred
    return responsescv.wait_for(lock, std::chrono::seconds(30), [this, requestNo]() {return responseArrived(requestNo);});
}

// Removes a request from responses map
void AutopowerClient::purgeRequestNo(int requestNo) {
    std::lock_guard<std::mutex> lock(responseslock);
    if (responses.find(requestNo) != responses.end()) {
        responses.erase(requestNo);
    }
}
