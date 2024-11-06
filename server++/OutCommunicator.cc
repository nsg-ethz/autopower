#include "OutCommunicator.h"
#include <chrono>
#include <jsoncpp/json/json.h>

void OutCommunicator::addMessageCli(const std::string& message) {
    std::lock_guard<std::mutex> lock(queueMutex);
    cliMessageQueue.push(message);
}

void OutCommunicator::addMessageOut(const std::string& message) {
    std::lock_guard<std::mutex> lock(queueMutex);
    Json::Value outMsg;
    outMsg["type"] = "message";
    outMsg["timestamp"] = static_cast<unsigned int>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    outMsg["content"] = message;

    for (auto& queuePair : outMessageQueues) {
        queuePair.second.push(outMsg.toStyledString());
    }
}

void OutCommunicator::addNewMessageOutListener(const std::string& managementUid) {
    std::lock_guard<std::mutex> lock(queueMutex);
    outMessageQueues[managementUid] = std::queue<std::string>();
}

void OutCommunicator::addMessage(const std::string& message) {
    addMessageCli(message);
    addMessageOut(message);
}

std::string OutCommunicator::getNextMessageCli() {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (cliMessageQueue.empty()) {
        return "";
    }
    std::string msg = cliMessageQueue.front();
    cliMessageQueue.pop();
    return msg;
}

std::string OutCommunicator::getNextMessageOut(const std::string& managementUid) {
    std::lock_guard<std::mutex> lock(queueMutex);
    auto& queue = outMessageQueues[managementUid];
    if (queue.empty()) {
        return "";
    }
    std::string msg = queue.front();
    queue.pop();
    return msg;
}
