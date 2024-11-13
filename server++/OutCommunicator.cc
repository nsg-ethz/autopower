#include "OutCommunicator.h"
#include <chrono>
#include <condition_variable>
#include <future>
#include <jsoncpp/json/json.h>
#include <mutex>
#include <tuple>

void OutCommunicator::addMessageCli(const std::string& message) {
    std::lock_guard<std::mutex> lock(queueMutex);
    cliMessageQueue.push(message);
    queueCondition.notify_all();
}

void OutCommunicator::addMessageOut(const std::string& message) {
    std::lock_guard<std::mutex> lock(queueMutex);
    // create json values for the output message
    Json::Value outMsg;
    
    outMsg["type"] = "message";
    outMsg["timestamp"] = static_cast<unsigned int>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    outMsg["content"] = message;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // to save space, do not have any indentation
    std::string messageString = Json::writeString(builder, outMsg);

    // put message to all queues. For now must happen under locking
    // TODO: Find out if this can also happen without exclusive locking. Needs investigation of internals of the std::unordered_map
    std::unique_lock<std::shared_mutex> qsMtx(queuesMutex);
    for (auto& queuePair : outMessageQueues) {
        // push entry to queue (queuePair.second = std::pair<std::condition_variable, std::queue>)
        std::tuple<std::condition_variable, std::mutex, std::queue<std::string>>* queueCVContentPtr = queuePair.second;
        std::unique_lock<std::mutex> queueLock(std::get<1>(*queueCVContentPtr));
        std::get<2>(*queueCVContentPtr).push(messageString);
        // notify listeners at this queue
        queueLock.unlock();
        std::get<0>(*queueCVContentPtr).notify_all();
    }

    queuesMutex.unlock();

}

void OutCommunicator::addNewMessageOutListener(const std::string& managementUid) {
    std::unique_lock<std::shared_mutex> lock(queuesMutex);
    outMessageQueues[managementUid] = new std::make_tuple(new std::condition_variable(), new std::mutex(), new std::queue<std::string>());;
}

void OutCommunicator::deleteMessageOutListener(const std::string& managementUid) {
    std::unique_lock<std::shared_mutex> lock(queuesMutex);
    std::tuple<std::condition_variable*, std::mutex*, std::queue<std::string>*> entry = *outMessageQueues[managementUid];
    std::condition_variable* cv = std::get<0>(entry);
    cv->notify_all(); // hope to finish work of all others
    std::mutex* mtxPtr = std::get<1>(entry);
    std::unique_lock<std::mutex>lk (*mtxPtr);
    delete cv;
    delete std::get<2>(entry); // delete queue
    lk.unlock();
    delete mtxPtr;
    delete outMessageQueues[managementUid];
}
void OutCommunicator::addMessage(const std::string& message) {
    addMessageCli(message);
    addMessageOut(message);
}

std::string OutCommunicator::getNextMessageCli() {
    std::unique_lock<std::mutex> lock(queueMutex);
    // wait until no longer empty
    queueCondition.wait(lock, [this]() { return !cliMessageQueue.empty(); });

    std::string msg = cliMessageQueue.front();
    cliMessageQueue.pop();
    return msg;
}

std::string OutCommunicator::getNextMessageOut(const std::string& managementUid) {
    std::shared_lock<std::shared_mutex> lock(queuesMutex);
    std::tuple<std::condition_variable*, std::mutex*, std::queue<std::string>*> queueTuple = *outMessageQueues[managementUid];
    
    std::condition_variable* cv = std::get<0>(queueTuple);
    std::mutex* qMtx = std::get<1>(queueTuple);
    std::queue<std::string>* queue = std::get<2>(queueTuple);
    std::unique_lock<std::mutex> thisQueueLock(*qMtx);

    // wait until no longer empty
    cv->wait(thisQueueLock, [this, queue]() {return !queue->empty();});
    std::string msg = queue->front();
    queue->pop();
    // queuesMutex will get unlocked here ONLY
    return msg;
}
