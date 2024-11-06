#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "PgConnectorFactory.h"
#include "pbdef/api_pb2.h"
#include <pqxx/pqxx>

class CMeasurementApiServicer {
public:
    CMeasurementApiServicer(std::shared_ptr<pgConnectorFactory> pgFactory, const std::unordered_map<std::string, std::string>& allowedMgmtClients);

    void logClientWasSeenNow(const std::string& clientUid);
    pbdef::api::nothing registerClient(const pbdef::api::clientRequest& request);
    pbdef::api::nothing putClientResponse(const pbdef::api::clientResponse& response);
    pbdef::api::nothing putMeasurementList(const std::vector<pbdef::api::measurementName>& requestList);
    pbdef::api::nothing putMeasurement(const std::vector<pbdef::api::measurement>& measurements);
    pbdef::api::msmtSettings getMsmtSttngsAndStart(const pbdef::api::clientRequest& request);
    pbdef::api::nothing putStatusMsg(const pbdef::api::statusMessage& request);
    bool isValidMgmtClient(const pbdef::api::mgmtClientRequest& request);
    std::vector<pbdef::api::clientUid> getLoggedInClients(const pbdef::api::clientRequest& request);
    pbdef::api::registrationStatus getRegistrationStatus(const pbdef::api::registrationRequest& request);
    pbdef::api::nothing setMsmtSttings(const pbdef::api::msmtSettingsRequest& request);
    pbdef::api::srvResponse issueRequestToClient(const pbdef::api::mgmtRequest& request);
    std::vector<pbdef::api::cmMCode> getMessages(const pbdef::api::mgmtRequest& request);

private:
    std::shared_ptr<pqxx::connection> pgConn_;
    std::shared_ptr<pgConnectorFactory> pgFactory_;
    std::unordered_map<std::string, std::string> allowedMgmtClients_;
};