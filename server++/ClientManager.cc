#include "ClientManager.h"

// Returns a list of client IDs.
std::vector<std::string> ClientManager::getLoggedInClientsList() {
    std::vector<std::string> clientIds;
    for (const auto& client : loggedInClients) {
        clientIds.push_back(client.first);
    }
    return clientIds;
}

// Checks if a client is in the logged-in clients list.
bool ClientManager::isInLoggedInClientsList(const std::string& id) {
    return loggedInClients.find(id) != loggedInClients.end();
}

// Gets the next request of a specific client.
Job ClientManager::getNextRequestOfClient(const std::string& clientId) {
    return loggedInClients.at(clientId).getNextRequest();
}

// Sets measurement settings for a specific client.
void ClientManager::setMsmtSettingsOfClient(const std::string& clientId, const std::string& ppdev, const std::string& sampleInt, int uploadInt) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot get settings of a non-registered client: " + clientId);
        return;
    }

    AutopowerClient& ec = loggedInClients[clientId];
    ec.setPpDevice(ppdev);
    ec.setPpSampleInterval(sampleInt);
    ec.setuploadIntervalMin(uploadInt);
}

// Retrieves the measurement settings for a client.
std::unordered_map<std::string, std::string> ClientManager::getMsmtSettingsOfClient(const std::string& clientId) {
    if (isInLoggedInClientsList(clientId)) {
        return loggedInClients.at(clientId).getMsmtSettings();
    }
    return {};
}

// Adds a new client.
void ClientManager::addNewClient(const std::string& clientId) {
    if (isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Re-registered already existing client. Deleting old one.");
        loggedInClients[clientId].scheduleDeletion();
    }

    loggedInClients[clientId] = AutopowerClient(clientId);
}

// Schedules a new request to a client.
int ClientManager::scheduleNewRequestToClient(const std::string& clientId, Job& job) {
    return loggedInClients.at(clientId).scheduleRequest(job);
}

// Adds a response to a specific request.
void ClientManager::addNewResponseToRequest(const std::string& clientId, const Response& response) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot handle response of non-existing client " + clientId + ". Please issue the last requests again.");
        return;
    }

    loggedInClients[clientId].setResponse(response);
}

// Gets the response for a specific request number.
Response ClientManager::getResponseOfRequestNo(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot wait on a request issued to a non-existing client " + clientId + ". Please try again later.");
        throw std::runtime_error("Request not available");
    }

    AutopowerClient& ec = loggedInClients[clientId];
    if (!ec.responseArrived(requestNo)) {
        OutCommunicator pm;
        pm.addMessage("Cannot get requestNo " + std::to_string(requestNo) + " for client " + clientId + " as it did not arrive yet");
        throw std::runtime_error("Response not available");
    }

    return ec.getResponse(requestNo);
}

// Waits for the completion of a specific request.
bool ClientManager::waitOnRequestCompletion(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot wait on a request issued to non-existing client " + clientId + ". Please try again later.");
        return false;
    }

    return loggedInClients[clientId].waitForResponseTo(requestNo);
}

// Purges the response of a specific request number.
bool ClientManager::purgeResponseOfRequestNo(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot purge request issued to non-existing client " + clientId + ". Please try again later.");
        return false;
    }

    loggedInClients[clientId].purgeRequestNo(requestNo);
    return true;
}

// Adds a synchronous request to a client and waits for its completion.
Response ClientManager::addSyncRequest(const std::string& clientId, Job& request) {
    int requestNo = scheduleNewRequestToClient(clientId, request);
    if (!waitOnRequestCompletion(clientId, requestNo)) {
        throw std::runtime_error("Failed to get confirmation from client. This may be a timeout error.");
    }

    Response response = getResponseOfRequestNo(clientId, requestNo);
    if (response.statusCode != 0) {
        throw std::runtime_error("Could not execute request successfully. Client issued error: " + response.msg);
    }

    purgeResponseOfRequestNo(clientId, requestNo);  // Clean up
    return response;
}
