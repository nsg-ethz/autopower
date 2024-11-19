#include "CMeasurementApiServicer.h"
#include "ClientManager.h" 
#include "OutCommunicator.h"
#include "api.pb.h"
#include "cmake/api.pb.h"
#include <grpcpp/server_context.h>
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
    // print measurement list from client to out communicator

    class Recorder : public grpc::ServerReadReactor<autopapi::msmtName> {
        public:
          Recorder() {
            oc = OutCommunicator();
            StartRead(&mname_);
          }

          void OnReadDone (bool ok) override {
            if (ok) {
                oc.addMessage(mname_.name());
                // successful read, continue
                logClientWasSeenNow(mname_.clientuid());
                StartRead(&mname_);
            } else {
                // finishing up
                Finish(grpc::Status::OK);
            }
          }

          void OnDone() override {
            delete this;
          }

          void OnCancel() override { /* Nothing to do since the rpc does not mutate state */};

          private:
            OutCommunicator oc;
            ::autopapi::msmtName mname_;
    };

    nth = autopapi::nothing(); // TODO: this should most likely be a new object?

    return new Recorder();
}

::grpc::ServerBidiReactor< ::autopapi::msmtSample, ::autopapi::sampleAck>* putMeasurement(::grpc::CallbackServerContext* ctxt) {
    class MeasurementSaveHandler : public grpc::ServerBidiReactor<::autopapi::msmtSample, ::autopapi::sampleAck>(::grpc::CallbackServerContext* ctx) {
        public:
          MeasurementSaveHandler(pqxx::connection& cnn) {
            cnn_ = cnn;
            txn_ = pqxx::work(cnn);
            
            txn.prepare("insMsmtData", "INSERT INTO measurement_data (server_measurement_id, measurement_value, measurement_timestamp)
                        VALUES(
                            (SELECT server_measurement_id FROM measurements WHERE shared_measurement_id = $1 LIMIT 1) -- handles only the server id
                        ,$2,$3) RETURNING $4 AS sampleid;");

            txn.prepare("createClientIfNeeded", "INSERT INTO clients (client_uid) VALUES ($1) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW();");
            // $1 = clientUid, $2 = measurementStartTimestamp, $3 = measurementId
            txn.prepare("createMeasurementIfNeeded", "WITH getRunIds AS (
                            SELECT runs.run_id AS run_id FROM runs, client_runs WHERE client_runs.client_uid = $1 AND runs.run_id = client_runs.run_id AND start_run <= $2 AND (stop_run >= $2 OR stop_run IS NULL)
                        ),
                        getRunId AS ( -- ensures that only one result can get back. Otherwise return NULL
                            SELECT CASE WHEN (COUNT(*) <> 1) THEN NULL ELSE (SELECT * FROM getRunIds LIMIT 1) END FROM getRunIds
                        )
                        INSERT INTO
                        measurements (shared_measurement_id, client_uid, run_id)
                        VALUES (
                            $3,
                            $1,
                            (SELECT * FROM getRunId LIMIT 1) -- will return NULL if there is no run id else the run id
                        )
                        ON CONFLICT DO NOTHING");
            msmtIdSoFar = ""; // for performance, we only execute certain queries if the measurement id changed
            StartRead(&smp);
          }

          void OnReadDone(bool ok) override {
            if (ok) {
                if (msmtIdSoFar != smp->msmtid()) {
                    txn.exec_prepared("createClientIfNeeded", smp->clientuid());
                    txn.exec_prepared("createMeasurementIfNeeded", smp->clientuid(), google::protobuf::util::TimeUtil::ToString(smp->msmttime()), smp->msmtid());
                    msmtIdSoFar = smp->msmtid();

                    CommitAndAck();
                }

                writeAndAckMtx->Lock();
                // write sample to DB. On conflict still succeed to avoid duplicates.
                // convert timestamp to UTC (TODO: Check if this is auto converted now)

                pqxx::result insertRes = txn.exec_prepared("insMsmtData", smp->msmtid(), smp->msmtcontent(), google::protobuf::util::TimeUtil::ToString(smp->msmttime()), smp->sampleid());
                savedMeasurementsToAck.append(insertRes[0]["sampleid"]);
                // save ack in local acking queue.
                writeAndAckMtx->Unlock();

                StartWrite(&smp);
            } else {
                Finish(Status::OK);
            }
          }

          void OnWriteDone(bool ok) override {NextWrite()};

          void OnDone() override {
            // commit written entries
            CommitAndAck();
            delete this;
          }

          void OnCancel() {
            std::cerr << "measurement upload received cancel request. The database may not be synchronized." << std::endl;
            txn_->rollback();
            // revert the transaction
          }
        private:
          ::autopapi::msmtSample smp;
          pqxx::connection cnn_;
          pqxx::work txn_;
          std::queue<uint64_t> savedMeasurementsToAck;
          std::string msmtIdSoFar;

          absl::mutex* writeAndAckMtx;
        void CommitAndAck() {
            // commits all entries written so far and then notifies the client that it has written the samples successfully.
            writeAndAckMtx->Lock();
            txn_.commit();
            while(!savedMeasurementsToAck.empty()) {
                // get all sample ids and create acks
                uint64_t sampleIdToAck = savedMeasurementsToAck.front();
                savedMeasurementsToAck.pop();

                ::autopapi::sampleAck ack();
                ack.set_sampleid(sampleIdToAck);
                StartWrite(&ack);
            }

            writeAndAckMtx->Unlock();
        }
    }

    return MeasurementSaveHandler(*pgConn_);
}

::grpc::ServerUnaryReactor* getMsmtSttngsAndStart(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientUid* cluid, ::autopapi::msmtSettings* mset) {
    ClientManager cm;
    if (cm.isInLoggedInClientsList(cluid->uid())) {
        struct msmtSettings msmtSettings = cm.getMsmtSettingsOfClient(cluid->uid());
        mset->set_ppdevice(msmtSettings.ppDevice);
        mset->set_ppsamplinginterval(msmtSettings.ppSamplingInterval);
        mset->set_uploadintervalmin(msmtSettings.uploadIntervalMin);
        logClientWasSeenNow(cluid->uid());
        auto* reactor = ctxt->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // not logged in client --> return error
    auto* reactor = ctxt->DefaultReactor();
    reactor->Finish(grpc::Status::NOT_FOUND);
    return reactor;

}

::grpc::ServerUnaryReactor* putStatusMsg(::grpc::CallbackServerContext* ctxt, const ::autopapi::cmMCode* cmde, ::autopapi::nothing* nth) {
    // Store an (error) message in the DB log
    ClientManager cm;
    OutCommunicator oc;
    
    // ::autopapi::nothing* nothing = new ::autopapi::nothing; // TODO: maybe need to alloc or not --> api
    // nth = nothing;

    if (cm.isInLoggedInClientsList(cmde->clientuid())) {
        if (cmde->statuscode() != 0) {
            pc.addMessage("Logging error from '" + cmde->clientuid() + "': " + cmde->msg());
            pqxx::work txn = pqxx::work(*pgConn_);
            // may need to create client if not exists
            txn.exec_params("INSERT INTO clients (client_uid) VALUES ($1) ON CONFLICT (client_uid) DO UPDATE SET last_seen = NOW();", cmde->clientuid());
            txn.exec_params("INSERT INTO logmessages (client_uid, error_code, log_message) VALUES ($1, $2, $3)", cmde->clientuid(), cmde->statuscode(), cmde->msg());
            txn.commit();
        } else {
            pc.addMessage("[Status] from '" + cmde->clientuid() + "': " + cmde->msg());
        }
        logClientWasSeenNow(cmde->clientuid());
        auto* reactor = ctxt->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

    // not logged in client --> return error
    auto* reactor = ctxt->DefaultReactor();
    reactor->Finish(grpc::Status::NOT_FOUND);
    return reactor;
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