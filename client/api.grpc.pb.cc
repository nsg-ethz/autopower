// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: api.proto

#include "api.pb.h"
#include "api.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
namespace autopapi {

static const char* CMeasurementApi_method_names[] = {
  "/autopapi.CMeasurementApi/registerClient",
  "/autopapi.CMeasurementApi/putClientResponse",
  "/autopapi.CMeasurementApi/putMeasurementList",
  "/autopapi.CMeasurementApi/putMeasurement",
  "/autopapi.CMeasurementApi/getMsmtSttngsAndStart",
  "/autopapi.CMeasurementApi/putStatusMsg",
  "/autopapi.CMeasurementApi/getLoggedInClients",
  "/autopapi.CMeasurementApi/getRegistrationStatus",
  "/autopapi.CMeasurementApi/setMsmtSttings",
  "/autopapi.CMeasurementApi/issueRequestToClient",
  "/autopapi.CMeasurementApi/getMessages",
};

std::unique_ptr< CMeasurementApi::Stub> CMeasurementApi::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< CMeasurementApi::Stub> stub(new CMeasurementApi::Stub(channel, options));
  return stub;
}

CMeasurementApi::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_registerClient_(CMeasurementApi_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::SERVER_STREAMING, channel)
  , rpcmethod_putClientResponse_(CMeasurementApi_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_putMeasurementList_(CMeasurementApi_method_names[2], options.suffix_for_stats(),::grpc::internal::RpcMethod::CLIENT_STREAMING, channel)
  , rpcmethod_putMeasurement_(CMeasurementApi_method_names[3], options.suffix_for_stats(),::grpc::internal::RpcMethod::BIDI_STREAMING, channel)
  , rpcmethod_getMsmtSttngsAndStart_(CMeasurementApi_method_names[4], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_putStatusMsg_(CMeasurementApi_method_names[5], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_getLoggedInClients_(CMeasurementApi_method_names[6], options.suffix_for_stats(),::grpc::internal::RpcMethod::SERVER_STREAMING, channel)
  , rpcmethod_getRegistrationStatus_(CMeasurementApi_method_names[7], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_setMsmtSttings_(CMeasurementApi_method_names[8], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_issueRequestToClient_(CMeasurementApi_method_names[9], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_getMessages_(CMeasurementApi_method_names[10], options.suffix_for_stats(),::grpc::internal::RpcMethod::SERVER_STREAMING, channel)
  {}

::grpc::ClientReader< ::autopapi::srvRequest>* CMeasurementApi::Stub::registerClientRaw(::grpc::ClientContext* context, const ::autopapi::clientUid& request) {
  return ::grpc::internal::ClientReaderFactory< ::autopapi::srvRequest>::Create(channel_.get(), rpcmethod_registerClient_, context, request);
}

void CMeasurementApi::Stub::async::registerClient(::grpc::ClientContext* context, const ::autopapi::clientUid* request, ::grpc::ClientReadReactor< ::autopapi::srvRequest>* reactor) {
  ::grpc::internal::ClientCallbackReaderFactory< ::autopapi::srvRequest>::Create(stub_->channel_.get(), stub_->rpcmethod_registerClient_, context, request, reactor);
}

::grpc::ClientAsyncReader< ::autopapi::srvRequest>* CMeasurementApi::Stub::AsyncregisterClientRaw(::grpc::ClientContext* context, const ::autopapi::clientUid& request, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::srvRequest>::Create(channel_.get(), cq, rpcmethod_registerClient_, context, request, true, tag);
}

::grpc::ClientAsyncReader< ::autopapi::srvRequest>* CMeasurementApi::Stub::PrepareAsyncregisterClientRaw(::grpc::ClientContext* context, const ::autopapi::clientUid& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::srvRequest>::Create(channel_.get(), cq, rpcmethod_registerClient_, context, request, false, nullptr);
}

::grpc::Status CMeasurementApi::Stub::putClientResponse(::grpc::ClientContext* context, const ::autopapi::clientResponse& request, ::autopapi::nothing* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::clientResponse, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_putClientResponse_, context, request, response);
}

void CMeasurementApi::Stub::async::putClientResponse(::grpc::ClientContext* context, const ::autopapi::clientResponse* request, ::autopapi::nothing* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::clientResponse, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_putClientResponse_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::putClientResponse(::grpc::ClientContext* context, const ::autopapi::clientResponse* request, ::autopapi::nothing* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_putClientResponse_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::PrepareAsyncputClientResponseRaw(::grpc::ClientContext* context, const ::autopapi::clientResponse& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::nothing, ::autopapi::clientResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_putClientResponse_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::AsyncputClientResponseRaw(::grpc::ClientContext* context, const ::autopapi::clientResponse& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncputClientResponseRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::ClientWriter< ::autopapi::msmtName>* CMeasurementApi::Stub::putMeasurementListRaw(::grpc::ClientContext* context, ::autopapi::nothing* response) {
  return ::grpc::internal::ClientWriterFactory< ::autopapi::msmtName>::Create(channel_.get(), rpcmethod_putMeasurementList_, context, response);
}

void CMeasurementApi::Stub::async::putMeasurementList(::grpc::ClientContext* context, ::autopapi::nothing* response, ::grpc::ClientWriteReactor< ::autopapi::msmtName>* reactor) {
  ::grpc::internal::ClientCallbackWriterFactory< ::autopapi::msmtName>::Create(stub_->channel_.get(), stub_->rpcmethod_putMeasurementList_, context, response, reactor);
}

::grpc::ClientAsyncWriter< ::autopapi::msmtName>* CMeasurementApi::Stub::AsyncputMeasurementListRaw(::grpc::ClientContext* context, ::autopapi::nothing* response, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::internal::ClientAsyncWriterFactory< ::autopapi::msmtName>::Create(channel_.get(), cq, rpcmethod_putMeasurementList_, context, response, true, tag);
}

::grpc::ClientAsyncWriter< ::autopapi::msmtName>* CMeasurementApi::Stub::PrepareAsyncputMeasurementListRaw(::grpc::ClientContext* context, ::autopapi::nothing* response, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncWriterFactory< ::autopapi::msmtName>::Create(channel_.get(), cq, rpcmethod_putMeasurementList_, context, response, false, nullptr);
}

::grpc::ClientReaderWriter< ::autopapi::msmtSample, ::autopapi::sampleAck>* CMeasurementApi::Stub::putMeasurementRaw(::grpc::ClientContext* context) {
  return ::grpc::internal::ClientReaderWriterFactory< ::autopapi::msmtSample, ::autopapi::sampleAck>::Create(channel_.get(), rpcmethod_putMeasurement_, context);
}

void CMeasurementApi::Stub::async::putMeasurement(::grpc::ClientContext* context, ::grpc::ClientBidiReactor< ::autopapi::msmtSample,::autopapi::sampleAck>* reactor) {
  ::grpc::internal::ClientCallbackReaderWriterFactory< ::autopapi::msmtSample,::autopapi::sampleAck>::Create(stub_->channel_.get(), stub_->rpcmethod_putMeasurement_, context, reactor);
}

::grpc::ClientAsyncReaderWriter< ::autopapi::msmtSample, ::autopapi::sampleAck>* CMeasurementApi::Stub::AsyncputMeasurementRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::internal::ClientAsyncReaderWriterFactory< ::autopapi::msmtSample, ::autopapi::sampleAck>::Create(channel_.get(), cq, rpcmethod_putMeasurement_, context, true, tag);
}

::grpc::ClientAsyncReaderWriter< ::autopapi::msmtSample, ::autopapi::sampleAck>* CMeasurementApi::Stub::PrepareAsyncputMeasurementRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncReaderWriterFactory< ::autopapi::msmtSample, ::autopapi::sampleAck>::Create(channel_.get(), cq, rpcmethod_putMeasurement_, context, false, nullptr);
}

::grpc::Status CMeasurementApi::Stub::getMsmtSttngsAndStart(::grpc::ClientContext* context, const ::autopapi::clientUid& request, ::autopapi::msmtSettings* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::clientUid, ::autopapi::msmtSettings, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_getMsmtSttngsAndStart_, context, request, response);
}

void CMeasurementApi::Stub::async::getMsmtSttngsAndStart(::grpc::ClientContext* context, const ::autopapi::clientUid* request, ::autopapi::msmtSettings* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::clientUid, ::autopapi::msmtSettings, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_getMsmtSttngsAndStart_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::getMsmtSttngsAndStart(::grpc::ClientContext* context, const ::autopapi::clientUid* request, ::autopapi::msmtSettings* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_getMsmtSttngsAndStart_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::msmtSettings>* CMeasurementApi::Stub::PrepareAsyncgetMsmtSttngsAndStartRaw(::grpc::ClientContext* context, const ::autopapi::clientUid& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::msmtSettings, ::autopapi::clientUid, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_getMsmtSttngsAndStart_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::msmtSettings>* CMeasurementApi::Stub::AsyncgetMsmtSttngsAndStartRaw(::grpc::ClientContext* context, const ::autopapi::clientUid& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncgetMsmtSttngsAndStartRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status CMeasurementApi::Stub::putStatusMsg(::grpc::ClientContext* context, const ::autopapi::cmMCode& request, ::autopapi::nothing* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::cmMCode, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_putStatusMsg_, context, request, response);
}

void CMeasurementApi::Stub::async::putStatusMsg(::grpc::ClientContext* context, const ::autopapi::cmMCode* request, ::autopapi::nothing* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::cmMCode, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_putStatusMsg_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::putStatusMsg(::grpc::ClientContext* context, const ::autopapi::cmMCode* request, ::autopapi::nothing* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_putStatusMsg_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::PrepareAsyncputStatusMsgRaw(::grpc::ClientContext* context, const ::autopapi::cmMCode& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::nothing, ::autopapi::cmMCode, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_putStatusMsg_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::AsyncputStatusMsgRaw(::grpc::ClientContext* context, const ::autopapi::cmMCode& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncputStatusMsgRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::ClientReader< ::autopapi::clientUid>* CMeasurementApi::Stub::getLoggedInClientsRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request) {
  return ::grpc::internal::ClientReaderFactory< ::autopapi::clientUid>::Create(channel_.get(), rpcmethod_getLoggedInClients_, context, request);
}

void CMeasurementApi::Stub::async::getLoggedInClients(::grpc::ClientContext* context, const ::autopapi::mgmtAuth* request, ::grpc::ClientReadReactor< ::autopapi::clientUid>* reactor) {
  ::grpc::internal::ClientCallbackReaderFactory< ::autopapi::clientUid>::Create(stub_->channel_.get(), stub_->rpcmethod_getLoggedInClients_, context, request, reactor);
}

::grpc::ClientAsyncReader< ::autopapi::clientUid>* CMeasurementApi::Stub::AsyncgetLoggedInClientsRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::clientUid>::Create(channel_.get(), cq, rpcmethod_getLoggedInClients_, context, request, true, tag);
}

::grpc::ClientAsyncReader< ::autopapi::clientUid>* CMeasurementApi::Stub::PrepareAsyncgetLoggedInClientsRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::clientUid>::Create(channel_.get(), cq, rpcmethod_getLoggedInClients_, context, request, false, nullptr);
}

::grpc::Status CMeasurementApi::Stub::getRegistrationStatus(::grpc::ClientContext* context, const ::autopapi::authClientUid& request, ::autopapi::registrationStatus* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::authClientUid, ::autopapi::registrationStatus, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_getRegistrationStatus_, context, request, response);
}

void CMeasurementApi::Stub::async::getRegistrationStatus(::grpc::ClientContext* context, const ::autopapi::authClientUid* request, ::autopapi::registrationStatus* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::authClientUid, ::autopapi::registrationStatus, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_getRegistrationStatus_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::getRegistrationStatus(::grpc::ClientContext* context, const ::autopapi::authClientUid* request, ::autopapi::registrationStatus* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_getRegistrationStatus_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::registrationStatus>* CMeasurementApi::Stub::PrepareAsyncgetRegistrationStatusRaw(::grpc::ClientContext* context, const ::autopapi::authClientUid& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::registrationStatus, ::autopapi::authClientUid, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_getRegistrationStatus_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::registrationStatus>* CMeasurementApi::Stub::AsyncgetRegistrationStatusRaw(::grpc::ClientContext* context, const ::autopapi::authClientUid& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncgetRegistrationStatusRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status CMeasurementApi::Stub::setMsmtSttings(::grpc::ClientContext* context, const ::autopapi::mgmtMsmtSettings& request, ::autopapi::nothing* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::mgmtMsmtSettings, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_setMsmtSttings_, context, request, response);
}

void CMeasurementApi::Stub::async::setMsmtSttings(::grpc::ClientContext* context, const ::autopapi::mgmtMsmtSettings* request, ::autopapi::nothing* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::mgmtMsmtSettings, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_setMsmtSttings_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::setMsmtSttings(::grpc::ClientContext* context, const ::autopapi::mgmtMsmtSettings* request, ::autopapi::nothing* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_setMsmtSttings_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::PrepareAsyncsetMsmtSttingsRaw(::grpc::ClientContext* context, const ::autopapi::mgmtMsmtSettings& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::nothing, ::autopapi::mgmtMsmtSettings, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_setMsmtSttings_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::nothing>* CMeasurementApi::Stub::AsyncsetMsmtSttingsRaw(::grpc::ClientContext* context, const ::autopapi::mgmtMsmtSettings& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncsetMsmtSttingsRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status CMeasurementApi::Stub::issueRequestToClient(::grpc::ClientContext* context, const ::autopapi::mgmtRequest& request, ::autopapi::clientResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::autopapi::mgmtRequest, ::autopapi::clientResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_issueRequestToClient_, context, request, response);
}

void CMeasurementApi::Stub::async::issueRequestToClient(::grpc::ClientContext* context, const ::autopapi::mgmtRequest* request, ::autopapi::clientResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::autopapi::mgmtRequest, ::autopapi::clientResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_issueRequestToClient_, context, request, response, std::move(f));
}

void CMeasurementApi::Stub::async::issueRequestToClient(::grpc::ClientContext* context, const ::autopapi::mgmtRequest* request, ::autopapi::clientResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_issueRequestToClient_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::autopapi::clientResponse>* CMeasurementApi::Stub::PrepareAsyncissueRequestToClientRaw(::grpc::ClientContext* context, const ::autopapi::mgmtRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::autopapi::clientResponse, ::autopapi::mgmtRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_issueRequestToClient_, context, request);
}

::grpc::ClientAsyncResponseReader< ::autopapi::clientResponse>* CMeasurementApi::Stub::AsyncissueRequestToClientRaw(::grpc::ClientContext* context, const ::autopapi::mgmtRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncissueRequestToClientRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::ClientReader< ::autopapi::cmMCode>* CMeasurementApi::Stub::getMessagesRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request) {
  return ::grpc::internal::ClientReaderFactory< ::autopapi::cmMCode>::Create(channel_.get(), rpcmethod_getMessages_, context, request);
}

void CMeasurementApi::Stub::async::getMessages(::grpc::ClientContext* context, const ::autopapi::mgmtAuth* request, ::grpc::ClientReadReactor< ::autopapi::cmMCode>* reactor) {
  ::grpc::internal::ClientCallbackReaderFactory< ::autopapi::cmMCode>::Create(stub_->channel_.get(), stub_->rpcmethod_getMessages_, context, request, reactor);
}

::grpc::ClientAsyncReader< ::autopapi::cmMCode>* CMeasurementApi::Stub::AsyncgetMessagesRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::cmMCode>::Create(channel_.get(), cq, rpcmethod_getMessages_, context, request, true, tag);
}

::grpc::ClientAsyncReader< ::autopapi::cmMCode>* CMeasurementApi::Stub::PrepareAsyncgetMessagesRaw(::grpc::ClientContext* context, const ::autopapi::mgmtAuth& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncReaderFactory< ::autopapi::cmMCode>::Create(channel_.get(), cq, rpcmethod_getMessages_, context, request, false, nullptr);
}

CMeasurementApi::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[0],
      ::grpc::internal::RpcMethod::SERVER_STREAMING,
      new ::grpc::internal::ServerStreamingHandler< CMeasurementApi::Service, ::autopapi::clientUid, ::autopapi::srvRequest>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::clientUid* req,
             ::grpc::ServerWriter<::autopapi::srvRequest>* writer) {
               return service->registerClient(ctx, req, writer);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::clientResponse, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::clientResponse* req,
             ::autopapi::nothing* resp) {
               return service->putClientResponse(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[2],
      ::grpc::internal::RpcMethod::CLIENT_STREAMING,
      new ::grpc::internal::ClientStreamingHandler< CMeasurementApi::Service, ::autopapi::msmtName, ::autopapi::nothing>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             ::grpc::ServerReader<::autopapi::msmtName>* reader,
             ::autopapi::nothing* resp) {
               return service->putMeasurementList(ctx, reader, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[3],
      ::grpc::internal::RpcMethod::BIDI_STREAMING,
      new ::grpc::internal::BidiStreamingHandler< CMeasurementApi::Service, ::autopapi::msmtSample, ::autopapi::sampleAck>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             ::grpc::ServerReaderWriter<::autopapi::sampleAck,
             ::autopapi::msmtSample>* stream) {
               return service->putMeasurement(ctx, stream);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[4],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::clientUid, ::autopapi::msmtSettings, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::clientUid* req,
             ::autopapi::msmtSettings* resp) {
               return service->getMsmtSttngsAndStart(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[5],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::cmMCode, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::cmMCode* req,
             ::autopapi::nothing* resp) {
               return service->putStatusMsg(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[6],
      ::grpc::internal::RpcMethod::SERVER_STREAMING,
      new ::grpc::internal::ServerStreamingHandler< CMeasurementApi::Service, ::autopapi::mgmtAuth, ::autopapi::clientUid>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::mgmtAuth* req,
             ::grpc::ServerWriter<::autopapi::clientUid>* writer) {
               return service->getLoggedInClients(ctx, req, writer);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[7],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::authClientUid, ::autopapi::registrationStatus, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::authClientUid* req,
             ::autopapi::registrationStatus* resp) {
               return service->getRegistrationStatus(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[8],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::mgmtMsmtSettings, ::autopapi::nothing, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::mgmtMsmtSettings* req,
             ::autopapi::nothing* resp) {
               return service->setMsmtSttings(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[9],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CMeasurementApi::Service, ::autopapi::mgmtRequest, ::autopapi::clientResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::mgmtRequest* req,
             ::autopapi::clientResponse* resp) {
               return service->issueRequestToClient(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CMeasurementApi_method_names[10],
      ::grpc::internal::RpcMethod::SERVER_STREAMING,
      new ::grpc::internal::ServerStreamingHandler< CMeasurementApi::Service, ::autopapi::mgmtAuth, ::autopapi::cmMCode>(
          [](CMeasurementApi::Service* service,
             ::grpc::ServerContext* ctx,
             const ::autopapi::mgmtAuth* req,
             ::grpc::ServerWriter<::autopapi::cmMCode>* writer) {
               return service->getMessages(ctx, req, writer);
             }, this)));
}

CMeasurementApi::Service::~Service() {
}

::grpc::Status CMeasurementApi::Service::registerClient(::grpc::ServerContext* context, const ::autopapi::clientUid* request, ::grpc::ServerWriter< ::autopapi::srvRequest>* writer) {
  (void) context;
  (void) request;
  (void) writer;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::putClientResponse(::grpc::ServerContext* context, const ::autopapi::clientResponse* request, ::autopapi::nothing* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::putMeasurementList(::grpc::ServerContext* context, ::grpc::ServerReader< ::autopapi::msmtName>* reader, ::autopapi::nothing* response) {
  (void) context;
  (void) reader;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::putMeasurement(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::autopapi::sampleAck, ::autopapi::msmtSample>* stream) {
  (void) context;
  (void) stream;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::getMsmtSttngsAndStart(::grpc::ServerContext* context, const ::autopapi::clientUid* request, ::autopapi::msmtSettings* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::putStatusMsg(::grpc::ServerContext* context, const ::autopapi::cmMCode* request, ::autopapi::nothing* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::getLoggedInClients(::grpc::ServerContext* context, const ::autopapi::mgmtAuth* request, ::grpc::ServerWriter< ::autopapi::clientUid>* writer) {
  (void) context;
  (void) request;
  (void) writer;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::getRegistrationStatus(::grpc::ServerContext* context, const ::autopapi::authClientUid* request, ::autopapi::registrationStatus* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::setMsmtSttings(::grpc::ServerContext* context, const ::autopapi::mgmtMsmtSettings* request, ::autopapi::nothing* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::issueRequestToClient(::grpc::ServerContext* context, const ::autopapi::mgmtRequest* request, ::autopapi::clientResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CMeasurementApi::Service::getMessages(::grpc::ServerContext* context, const ::autopapi::mgmtAuth* request, ::grpc::ServerWriter< ::autopapi::cmMCode>* writer) {
  (void) context;
  (void) request;
  (void) writer;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace autopapi

