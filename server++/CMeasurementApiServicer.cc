#include "CMeasurementApiServicer.h"
#include "ClientManager.h" 
#include "OutCommunicator.h"
#include "cmake/api.pb.h"
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/status.h>
#include <netinet/in.h>
#include <pqxx/pqxx>
#include <jsoncpp/json/json.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>

CMeasurementApiServicer::CMeasurementApiServicer(std::unique_ptr<PgConnectorFactory> pgFactory, const std::unordered_map<std::string, std::string>& allowedMgmtClients)
    : pgFactory_(pgFactory), allowedMgmtClients_(allowedMgmtClients) {
    pgConn_ = pgFactory_->createConnection();
}

// convinience methods
void CMeasurementApiServicer::logClientWasSeenNow(const std::string& clientUid) {
    pqxx::work txn(*pgConn_);
    txn.exec_params("UPDATE clients SET last_seen = NOW() WHERE client_uid = $1", clientUid);
    txn.commit();
}

bool CMeasurementApiServicer::isValidMgmtClient(const autopapi::mgmtRequest& request) {
    // TODO: Fix this!!!
   /*
           if not request or not request.mgmtId or not request.pw:
            # For invalid request always deny
            print("WARNING: Got invalid request with auth for management method. Ignoring.")
            return False
        if request.mgmtId in self.allowedMgmtClients and pwdCtxt.verify(request.pw, self.allowedMgmtClients[request.mgmtId]):
            return True
        else:
            print("WARNING: Login failed due to invalid authentication paramaters. Ignoring.")
            return False
            */
    return true; 
}

// implementation
::grpc::ServerWriteReactor< ::autopapi::srvRequest>* registerClient(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientUid* cluid) {

    // Create custom class as in https://grpc.io/docs/languages/cpp/callback/. This serves as start to rewrite the API in a more callback friendly way in future.

    class ClientSender : public grpc::ServerWriteReactor<::autopapi::srvRequest> {
        public:
          ClientSender(const ::autopapi::clientUid* uid)
          :uid_(uid)
          {
            // add this as new client
            ClientManager cm_;
            cm_.addNewClient(uid_->uid());
            pqxx::work txn(*pgConn_);
            txn.exec_params("INSERT INTO clients (client_uid) VALUES ($1) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW()", uid_.uid());
            txn.commit();
            // inform about new client
            OutCommunicator pm;
            pm.addMessage("UID '" + uid_->uid() + "' registered");
            NextWrite();
          }
          void OnWriteDone(bool ok) override {
            if (!ok) {
              Finish(grpc::Status(grpc::StatusCode::UNKNOWN, "Unexpected Failure"));
            }
            NextWrite();
          }

        void OnDone() override {
            OutCommunicator oc;
            oc.addMessage("UID " + uid_->uid() + " lost connection.");
            delete this;
        }

        private:
          const ::autopapi::clientUid* uid_;
          
          void NextWrite() {
            // wait on next requests (for now with while loop)
            ClientManager cm_;
            while (true) {
                ::autopapi::srvRequest* rq = cm_.getNextRequestOfClient(uid_.uid());
                StartWrite(rq);
                delete rq; // this request is now invalid and can thus be deleted
            }

            Finish(grpc::Status::CANCELLED);
          };
    };

    return new ClientSender(cluid);
}

::grpc::ServerUnaryReactor* CMeasurementApiServicer::putClientResponse(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientResponse& response) {
    ClientManager cm;

    if (response.msgtype() == autopapi::clientResponseType::INTRODUCE_CLIENT) {
        // TODO: for now, give back the whole peer (with protocol and port)
        std::string ip = ctxt->peer().substr(5);  // Remove IPv4/IPv6 prefix
        Json::Value resultObj;
        resultObj["peerURI"] = ip;
        resultObj["msg"] = response.msg();
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        response.set_msg(Json::writeString(builder, resultObj));
    }

    cm.addNewResponseToRequest(response.clientuid(), response);

    logClientWasSeenNow(response.clientuid());
    auto* reactor = ctxt->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

::grpc::ServerReadReactor<::autopapi::msmtName>* putMeasurementList(::grpc::CallbackServerContext* ctxt, ::autopapi::nothing* nth) {
    pqxx::work txn(*pgConn_);
    for (const auto& measurementName : requestList) {
        txn.exec_params("INSERT INTO measurement_names (name) VALUES ($1) ON CONFLICT (name) DO NOTHING", measurementName.name());
    }
    txn.commit();
    return pbdef::api::nothing();
}

::grpc::ServerBidiReactor< ::autopapi::msmtSample, ::autopapi::sampleAck>* putMeasurement(::grpc::CallbackServerContext* ctxt) {
    // TODO
}

::grpc::ServerUnaryReactor* getMsmtSttngsAndStart(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientUid* cluid, ::autopapi::msmtSettings* mset) {
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

::grpc::ServerUnaryReactor* putStatusMsg(::grpc::CallbackServerContext* ctxt, const ::autopapi::cmMCode* cmde, ::autopapi::nothing* nth) {
    ClientManager cm;
    cm.updateClientStatus(request.client_uid(), request.msg());
    logClientWasSeenNow(request.client_uid());
    return pbdef::api::nothing();
}

::grpc::ServerWriteReactor< ::autopapi::clientUid>* getLoggedInClients(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtAuth* mmauth) {
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

::grpc::ServerUnaryReactor* getRegistrationStatus(::grpc::CallbackServerContext* ctxt, const ::autopapi::authClientUid* cluid, ::autopapi::registrationStatus* regstatus) {
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

::grpc::ServerUnaryReactor* setMsmtSttings(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtMsmtSettings* mset, ::autopapi::nothing* nth) {
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

::grpc::ServerUnaryReactor* issueRequestToClient(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtRequest* mgrequest, ::autopapi::clientResponse* cresp) {
    ClientManager cm;
    cm.issueRequestToClient(request.client_uid(), request);
    logClientWasSeenNow(request.client_uid());

    pbdef::api::srvResponse response;
    response.set_status("Request issued successfully.");
    return response;
}

::grpc::ServerWriteReactor< ::autopapi::cmMCode>* getMessages(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtAuth* mgmtauth) {
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