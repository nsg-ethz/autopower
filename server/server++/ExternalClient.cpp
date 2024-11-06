// ExternalClient.cpp
#include <map>
#include <string>
#include "ExternalClient.h"

ExternalClient::ExternalClient(const std::string& uid)
    : uid(uid), lastRequestNo(0), ppDevice("MCP1"), ppSamplingInterval("500"), uploadIntervalMin(5) {}

std::map<std::string, std::string> ExternalClient::getMsmtSettings() const {
    return {
        {"ppDevice", ppDevice},
        {"ppSamplingInterval", ppSamplingInterval},
        {"uploadIntervalMin", std::to_string(uploadIntervalMin)}
    };
}
// Adds a job to the job queue with an incremented request number.
int ExternalClient::scheduleRequest(Job& job) {
    std::lock_guard<std::mutex> lock(seqnoLock);
    lastRequestNo++;
    job.requestNo = lastRequestNo;
    jobqueue.push(job);
    return lastRequestNo;
}

// Retrieves the next job from the queue, blocking if empty.
Job ExternalClient::getNextRequest() {
    std::unique_lock<std::mutex> lock(jobQueueMutex);
    jobQueueCondition.wait(lock, [this]() { return !jobqueue.empty(); });
    Job nextJob = jobqueue.front();
    jobqueue.pop();
    return nextJob;
}

// Sets a response in the response dictionary and notifies all waiting threads.
void ExternalClient::setResponse(const Response& response) {
    std::lock_guard<std::mutex> lock(responsedictlock);
    responsedict[response.requestNo] = response;
    responsedictcv.notify_all();
}

// Checks if the response for a specific request number has arrived.
bool ExternalClient::responseArrived(int requestNo) const {
    std::lock_guard<std::mutex> lock(responsedictlock);
    return responsedict.find(requestNo) != responsedict.end() && responsedict.at(requestNo) != nullptr;
}

// Retrieves the response for a given request number if it has arrived, otherwise throws an error.
Response ExternalClient::getResponse(int requestNo) const {
    std::lock_guard<std::mutex> lock(responsedictlock);
    if (!responseArrived(requestNo)) {
        throw std::invalid_argument("Cannot call getResponse() on non-arrived requestNo " + std::to_string(requestNo));
    }
    return responsedict.at(requestNo);
}

// Waits for the response to arrive for a specified request number with a timeout.
bool ExternalClient::waitForResponseTo(int requestNo) {
    std::unique_lock<std::mutex> lock(responsedictlock);
    while (!responseArrived(requestNo)) {
        if (!responsedictcv.wait_for(lock, std::chrono::seconds(30))) {
            return false;  // Timeout occurred
        }
    }
    return true;
}

// Removes a request from the response dictionary if it exists.
void ExternalClient::purgeRequestNo(int requestNo) {
    std::lock_guard<std::mutex> lock(responsedictlock);
    responsedict.erase(requestNo);
}
