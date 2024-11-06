#include "CMeasurementApiServicer.h"
#include "ClientManager.h" // Assuming ClientManager is another module
#include "OutCommunicator.h"
#include <pqxx/pqxx>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <ipaddress.h> // assuming the IP handling library is included

CMeasurementApiServicer::CMeasurementApiServicer(std::shared_ptr<pgConnectorFactory> pgFactory, const std::unordered_map<std::string, std::string>& allowedMgmtClients)
    : pgFactory_(pgFactory), allowedMgmtClients_(allowedMgmtClients) {
    pgConn_ = pgFactory_->createConnection();
}

void CMeasurementApiServicer::logClientWasSeenNow(const std::string& clientUid) {
    pqxx::work txn(*pgConn_);
    txn.exec_params("UPDATE clients SET last_seen = NOW() WHERE client_uid = $1", clientUid);
    txn.commit();
}

pbdef::api::nothing CMeasurementApiServicer::registerClient(const pbdef::api::clientRequest& request) {
    ClientManager cmAdd;
    OutCommunicator pm;

    cmAdd.addNewClient(request.uid());
    pqxx::work txn(*pgConn_);
    txn.exec_params("INSERT INTO clients (client_uid) VALUES ($1) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW()", request.uid());
    txn.commit();

    pm.addMessage("UID '" + request.uid() + "' registered");

    // Simulate request loop similar to Python code
    while (true) {
        std::optional<pbdef::api::srvRequest> req = cmAdd.getNextRequestOfClient(request.uid());
        if (!req) {
            break;
        }
        return req.value();  // In C++, we don't use yield; instead, return or append responses
    }
    return pbdef::api::nothing();
}

pbdef::api::nothing CMeasurementApiServicer::putClientResponse(const pbdef::api::clientResponse& response) {
    ClientManager cm;

    if (response.msg_type() == pbdef::api::clientResponseType::INTRODUCE_CLIENT) {
        std::string ip = context.peer().substr(5);  // Remove IPv4/IPv6 prefix
        try {
            auto remoteIp = ipaddress::make_address(ip);
            nlohmann::json result = {{"peer", remoteIp.to_string()}, {"msg", response.msg()}};
            response.set_msg(result.dump());
        } catch (std::exception& e) {
            std::cerr << "Error: Could not parse IP: " << e.what() << std::endl;
        }
    }

    cm.addNewResponseToRequest(response.client_uid(), response);
    logClientWasSeenNow(response.client_uid());
    return pbdef::api::nothing();
}

void CMeasurementApiServicer::logClientWasSeenNow(const std::string& clientUid) {
    pqxx::work txn(*pgConn_);
    txn.exec_params("UPDATE clients SET last_seen = NOW() WHERE client_uid = $1", clientUid);
    txn.commit();
}

// `registerClient` method already implemented above

pbdef::api::nothing CMeasurementApiServicer::putMeasurementList(const std::vector<pbdef::api::measurementName>& requestList) {
    pqxx::work txn(*pgConn_);
    for (const auto& measurementName : requestList) {
        txn.exec_params("INSERT INTO measurement_names (name) VALUES ($1) ON CONFLICT (name) DO NOTHING", measurementName.name());
    }
    txn.commit();
    return pbdef::api::nothing();
}

pbdef::api::nothing CMeasurementApiServicer::putMeasurement(const std::vector<pbdef::api::measurement>& measurements) {
    pqxx::work txn(*pgConn_);
    for (const auto& measurement : measurements) {
        txn.exec_params(
            "INSERT INTO measurements (client_uid, name, value, timestamp) VALUES ($1, $2, $3, $4)",
            measurement.client_uid(), measurement.name(), measurement.value(), measurement.timestamp());
    }
    txn.commit();
    return pbdef::api::nothing();
}

pbdef::api::msmtSettings CMeasurementApiServicer::getMsmtSttngsAndStart(const pbdef::api::clientRequest& request) {
    pbdef::api::msmtSettings settings;
    pqxx::work txn(*pgConn_);
    pqxx::result r = txn.exec_params("SELECT setting_key, setting_value FROM measurement_settings WHERE client_uid = $1", request.uid());

    for (auto row : r) {
        auto* setting = settings.add_settings();
        setting->set_key(row["setting_key"].c_str());
        setting->set_value(row["setting_value"].c_str());
    }
    txn.commit();
    logClientWasSeenNow(request.uid());
    return settings;
}

pbdef::api::nothing CMeasurementApiServicer::putStatusMsg(const pbdef::api::statusMessage& request) {
    ClientManager cm;
    cm.updateClientStatus(request.client_uid(), request.msg());
    logClientWasSeenNow(request.client_uid());
    return pbdef::api::nothing();
}

bool CMeasurementApiServicer::isValidMgmtClient(const pbdef::api::mgmtClientRequest& request) {
    auto clientIt = allowedMgmtClients_.find(request.client_id());
    return clientIt != allowedMgmtClients_.end() && clientIt->second == request.client_secret();
}

std::vector<pbdef::api::clientUid> CMeasurementApiServicer::getLoggedInClients(const pbdef::api::clientRequest& request) {
    std::vector<pbdef::api::clientUid> clients;
    pqxx::work txn(*pgConn_);
    pqxx::result r = txn.exec("SELECT client_uid FROM clients WHERE logged_in = true");

    for (auto row : r) {
        pbdef::api::clientUid clientUid;
        clientUid.set_uid(row["client_uid"].c_str());
        clients.push_back(clientUid);
    }
    txn.commit();
    logClientWasSeenNow(request.uid());
    return clients;
}

pbdef::api::registrationStatus CMeasurementApiServicer::getRegistrationStatus(const pbdef::api::registrationRequest& request) {
    pbdef::api::registrationStatus status;
    pqxx::work txn(*pgConn_);
    pqxx::result r = txn.exec_params("SELECT status FROM registrations WHERE client_uid = $1", request.client_uid());

    if (!r.empty()) {
        status.set_status(r[0]["status"].c_str());
    }
    txn.commit();
    logClientWasSeenNow(request.client_uid());
    return status;
}

pbdef::api::nothing CMeasurementApiServicer::setMsmtSttings(const pbdef::api::msmtSettingsRequest& request) {
    pqxx::work txn(*pgConn_);
    for (const auto& setting : request.settings()) {
        txn.exec_params(
            "INSERT INTO measurement_settings (client_uid, setting_key, setting_value) VALUES ($1, $2, $3) "
            "ON CONFLICT (client_uid, setting_key) DO UPDATE SET setting_value = $3",
            request.client_uid(), setting.key(), setting.value());
    }
    txn.commit();
    logClientWasSeenNow(request.client_uid());
    return pbdef::api::nothing();
}

pbdef::api::srvResponse CMeasurementApiServicer::issueRequestToClient(const pbdef::api::mgmtRequest& request) {
    ClientManager cm;
    cm.issueRequestToClient(request.client_uid(), request);
    logClientWasSeenNow(request.client_uid());

    pbdef::api::srvResponse response;
    response.set_status("Request issued successfully.");
    return response;
}

std::vector<pbdef::api::cmMCode> CMeasurementApiServicer::getMessages(const pbdef::api::mgmtRequest& request) {
    ClientManager cm;
    std::vector<pbdef::api::cmMCode> messages;

    for (const auto& message : cm.getMessagesOfClient(request.client_uid())) {
        pbdef::api::cmMCode msg;
        msg.set_msg(message);
        messages.push_back(msg);
    }
    logClientWasSeenNow(request.client_uid());
    return messages;
}