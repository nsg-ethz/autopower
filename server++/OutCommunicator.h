#pragma once

#include <queue>
#include <unordered_map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

class OutCommunicator {
private:
    std::queue<std::string> cliMessageQueue; // a queue saving error/status messages
    std::unordered_map<
      std::string, 
      std::tuple<std::condition_variable*, std::mutex*, std::queue<std::string>*>*
      > outMessageQueues; // a set of condition vars + queues saving error/status messages for the out communication only. Here for further features
    std::shared_mutex queuesMutex;
    std::mutex queueMutex;
    std::condition_variable queueCondition;


public:
    void addMessageCli(const std::string& message);
    void addMessageOut(const std::string& message);
    void addNewMessageOutListener(const std::string& managementUid);
    void deleteMessageOutListener(const std::string& managementUid);
    void addMessage(const std::string& message);
    std::string getNextMessageCli();
    std::string getNextMessageOut(const std::string& managementUid);
};