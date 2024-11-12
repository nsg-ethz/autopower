#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include "AutopowerClient.h"
#include "OutCommunicator.h"
#include "api.pb.h"

class ClientManager {
private:
    std::unordered_map<std::string, AutopowerClient*> loggedInClients;

public:
    std::vector<std::string> getLoggedInClientsList();
    bool isInLoggedInClientsList(const std::string& id);
    autopapi::srvRequest getNextRequestOfClient(const std::string& clientId);
    void setMsmtSettingsOfClient(const std::string& clientId, const std::string& ppdev, const std::string& sampleInt, int uploadInt);
    std::unordered_map<std::string, std::string> getMsmtSettingsOfClient(const std::string& clientId);
    void addNewClient(const std::string& clientId);
    int scheduleNewRequestToClient(const std::string& clientId, autopapi::srvRequest& job);
    void addNewResponseToRequest(const std::string& clientId, const autopapi::clientResponse& response);
    autopapi::clientResponse getResponseOfRequestNo(const std::string& clientId, int requestNo);
    bool waitOnRequestCompletion(const std::string& clientId, int requestNo);
    bool purgeResponseOfRequestNo(const std::string& clientId, int requestNo);
    autopapi::clientResponse addSyncRequest(const std::string& clientId, autopapi::srvRequest& request);
};