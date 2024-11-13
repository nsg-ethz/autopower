#pragma once

#include <grpcpp/server_context.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/sync_stream.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "PgConnectorFactory.h"
#include "api.pb.h"
#include "api.grpc.pb.h"
#include "cmake/api.grpc.pb.h"
#include "cmake/api.pb.h"
#include <pqxx/pqxx>

class CMeasurementApiServicer final : public autopapi::CMeasurementApi::CallbackService {
public:
    CMeasurementApiServicer(std::unique_ptr<PgConnectorFactory> pgFactory, const std::unordered_map<std::string, std::string>& allowedMgmtClients);
    ::grpc::ServerWriteReactor< ::autopapi::srvRequest>* registerClient(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientUid* cluid);
    ::grpc::ServerUnaryReactor* putClientResponse(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientResponse& response);
    ::grpc::ServerReadReactor<::autopapi::msmtName>* putMeasurementList(::grpc::CallbackServerContext* ctxt, ::autopapi::nothing* nth);
    ::grpc::ServerBidiReactor< ::autopapi::msmtSample, ::autopapi::sampleAck>* putMeasurement(::grpc::CallbackServerContext* ctxt);
    ::grpc::ServerUnaryReactor* getMsmtSttngsAndStart(::grpc::CallbackServerContext* ctxt, const ::autopapi::clientUid* cluid, ::autopapi::msmtSettings* mset);
    ::grpc::ServerUnaryReactor* putStatusMsg(::grpc::CallbackServerContext* ctxt, const ::autopapi::cmMCode* cmde, ::autopapi::nothing* nth);
    ::grpc::ServerWriteReactor< ::autopapi::clientUid>* getLoggedInClients(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtAuth* mmauth);
    ::grpc::ServerUnaryReactor* getRegistrationStatus(::grpc::CallbackServerContext* ctxt, const ::autopapi::authClientUid* cluid, ::autopapi::registrationStatus* regstatus);
    ::grpc::ServerUnaryReactor* setMsmtSttings(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtMsmtSettings* mset, ::autopapi::nothing* nth);
    ::grpc::ServerUnaryReactor* issueRequestToClient(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtRequest* mgrequest, ::autopapi::clientResponse* cresp);
    ::grpc::ServerWriteReactor< ::autopapi::cmMCode>* getMessages(::grpc::CallbackServerContext* ctxt, const ::autopapi::mgmtAuth* mgmtauth);

private:
    std::unique_ptr<pqxx::connection> pgConn_;
    std::shared_ptr<PgConnectorFactory> pgFactory_;
    std::unordered_map<std::string, std::string> allowedMgmtClients_;
    void logClientWasSeenNow(const std::string& clientUid);
    bool isValidMgmtClient(const autopapi::mgmtRequest& request);
};