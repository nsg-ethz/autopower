#include "ClientManager.h"
#include "api.pb.h"

// Returns a list of client IDs.
std::vector<std::string> ClientManager::getLoggedInClientsList() {
    std::vector<std::string> clientIds;
    for (const auto& clientDef : loggedInClients) {
        // See: https://stackoverflow.com/questions/8483985/obtaining-list-of-keys-and-values-from-unordered-map
        // seems to be the most portable way
        clientIds.push_back(clientDef.first);
    }
    return clientIds;
}

// Checks if a client is logged in
bool ClientManager::isInLoggedInClientsList(const std::string& id) {
    return loggedInClients.find(id) != loggedInClients.end();
}

// Gets the next request to send of a specific client.
autopapi::srvRequest* ClientManager::getNextRequestOfClient(const std::string& clientId) {
    return loggedInClients.at(clientId)->getNextRequest();
}

// Sets measurement settings for a specific client.
void ClientManager::setMsmtSettingsOfClient(const std::string& clientId, const std::string& ppdev, const std::string& sampleInt, int uploadInt) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator oc;
        oc.addMessage("Cannot get settings of a non-registered client: " + clientId);
        return;
    }

    AutopowerClient& ac = loggedInClients[clientId];
    ac.setPpDevice(ppdev);
    ac.setPpSampleInterval(sampleInt);
    ac.setUploadIntervalMin(uploadInt);
}

// Retrieves the measurement settings for a client.
std::unordered_map<std::string, std::string> ClientManager::getMsmtSettingsOfClient(const std::string& clientId) {
    if (isInLoggedInClientsList(clientId)) {
        // measurement can start --> return measurement settings to client.
        return loggedInClients.at(clientId).getMsmtSettings();
    }
    return {}; // non logged in client has no measurement settings. Will be handled in getMsmtSttngsAndStart of CMeasurementApiServicer
}

// Adds a new client.
void ClientManager::addNewClient(const std::string& clientId) {
    if (isInLoggedInClientsList(clientId)) {
        OutCommunicator oc;
        oc.addMessage("Re-registered already existing client. Deleting old one.");
        loggedInClients[clientId]->scheduleDeletion();
        delete loggedInClients[clientId]; // no longer needed. We'll add a new AutopowerClient
    }

    loggedInClients[clientId] = new AutopowerClient(clientId);
}

// Schedules a new request to a client.
int ClientManager::scheduleNewRequestToClient(const std::string& clientId, autopapi::srvRequest& job) {
    return loggedInClients.at(clientId)->scheduleRequest(job); // returns requestNo
}

// Adds a response to a specific request at some specific client
void ClientManager::addNewResponseToRequest(const std::string& clientId, const autopapi::clientResponse& response) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator oc;
        oc.addMessage("Cannot handle response of non-existing client " + clientId + ". Please issue the last requests again.");
        return;
    }

    loggedInClients[clientId]->setResponse(response);
}

// Gets the response for a specific request number.
autopapi::clientResponse ClientManager::getResponseOfRequestNo(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator oc;
        // TODO refactor out message
        oc.addMessage("Cannot wait on a request issued to a non-existing client " + clientId + ". Please try again later.");
        // TODO: catch exceptions on callers
        throw std::runtime_error("Cannot wait on a request issued to a non-existing client " + clientId + ". Please try again later.");
    }

    AutopowerClient* apclient = loggedInClients[clientId];
    if (!apclient->responseArrived(requestNo)) {
        OutCommunicator oc;
        oc.addMessage("Cannot get requestNo " + std::to_string(requestNo) + " for client " + clientId + " as it did not arrive yet");
        throw std::runtime_error("Cannot get requestNo " + std::to_string(requestNo) + " for client " + clientId + " as it did not arrive yet");
    }

    return apclient->getResponse(requestNo);
}

// Waits for the completion of a specific request.
bool ClientManager::waitOnRequestCompletion(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator pm;
        pm.addMessage("Cannot wait on a request issued to non-existing client " + clientId + ". Please try again later.");
        return false;
    }
    // true if no timeout occurred
    return loggedInClients[clientId]->waitForResponseTo(requestNo);
}

// Purges the response of a specific request number.
bool ClientManager::purgeResponseOfRequestNo(const std::string& clientId, int requestNo) {
    if (!isInLoggedInClientsList(clientId)) {
        OutCommunicator oc;
        oc.addMessage("Cannot purge request issued to non-existing client " + clientId + ". Please try again later.");
        return false;
    }

    loggedInClients[clientId]->purgeRequestNo(requestNo);
    return true;
}

// Adds a synchronous request to a client and waits for its completion.
autopapi::clientResponse ClientManager::addSyncRequest(const std::string& clientId, autopapi::srvRequest& request) {
    int requestNo = scheduleNewRequestToClient(clientId, request);
    if (!waitOnRequestCompletion(clientId, requestNo)) {
        throw std::runtime_error("Failed to get confirmation from client. This may be a timeout error.");
    }

    autopapi::clientResponse response = getResponseOfRequestNo(clientId, requestNo);
    if (response.statuscode() != 0) {
        throw std::runtime_error("Could not execute request successfully. Client issued error: " + response.msg());
    }

    purgeResponseOfRequestNo(clientId, requestNo);  // Clean up
    return response;
}
