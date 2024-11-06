#pragma once

#include <queue>
#include <unordered_map>
#include <string>
#include <mutex>

class OutCommunicator {
private:
    std::queue<std::string> cliMessageQueue;
    std::unordered_map<std::string, std::queue<std::string>> outMessageQueues;
    std::mutex queueMutex;

public:
    void addMessageCli(const std::string& message);
    void addMessageOut(const std::string& message);
    void addNewMessageOutListener(const std::string& managementUid);
    void addMessage(const std::string& message);
    std::string getNextMessageCli();
    std::string getNextMessageOut(const std::string& managementUid);
};