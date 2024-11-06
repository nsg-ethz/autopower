#include <unordered_map>
#include <string>
#include <vector>
#include "ExternalClient.h"
#include "OutCommunicator.h"

class ClientManager {
private:
    std::unordered_map<std::string, ExternalClient> loggedInClients;

public:
    std::vector<std::string> getLoggedInClientsList();
    bool isInLoggedInClientsList(const std::string& id);
    Job getNextRequestOfClient(const std::string& clientId);
    void setMsmtSettingsOfClient(const std::string& clientId, const std::string& ppdev, const std::string& sampleInt, int uploadInt);
    std::unordered_map<std::string, std::string> getMsmtSettingsOfClient(const std::string& clientId);
    void addNewClient(const std::string& clientId);
    int scheduleNewRequestToClient(const std::string& clientId, Job& job);
    void addNewResponseToRequest(const std::string& clientId, const Response& response);
    Response getResponseOfRequestNo(const std::string& clientId, int requestNo);
    bool waitOnRequestCompletion(const std::string& clientId, int requestNo);
    bool purgeResponseOfRequestNo(const std::string& clientId, int requestNo);
    Response addSyncRequest(const std::string& clientId, Job& request);
};