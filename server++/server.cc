#include <grpcpp/grpcpp.h>
#include <jsoncpp/json/json.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <thread>
#include "CLI.h"
#include "CMeasurementApiServicer.h"
#include "PgConnectorFactory.h"
#include "server.h"

void serve(const Json::Value& secrets, const Json::Value& config, bool interactive) {
    // Create gRPC server with thread pool
    grpc::ServerBuilder builder;
    std::shared_ptr<grpc::ServerCredentials> serverCreds;

    // SSL setup
    if (secrets.isMember("ssl")) {
        // Load private key
        if (!secrets["ssl"].isMember("privKeyPath")) {
            std::cerr << "ERROR: privKeyPath is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::ifstream privKeyFile(secrets["ssl"]["privKeyPath"].asString(), std::ios::binary);
        std::string privKey((std::istreambuf_iterator<char>(privKeyFile)), std::istreambuf_iterator<char>());

        // Load certificate chain
        if (!secrets["ssl"].isMember("pubKeyPath")) {
            std::cerr << "ERROR: pubKeyPath is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::ifstream certChainFile(secrets["ssl"]["pubKeyPath"].asString(), std::ios::binary);
        std::string certChain((std::istreambuf_iterator<char>(certChainFile)), std::istreambuf_iterator<char>());

        // Load CA certificate
        if (!secrets["ssl"].isMember("pubKeyCA")) {
            std::cerr << "ERROR: pubKeyCA is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::ifstream caFile(secrets["ssl"]["pubKeyCA"].asString(), std::ios::binary);
        std::string clientCa((std::istreambuf_iterator<char>(caFile)), std::istreambuf_iterator<char>());

        // Set up secure credentials
        grpc::SslServerCredentialsOptions::PemKeyCertPair keyCertPair = {privKey, certChain};
        grpc::SslServerCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = clientCa;
        sslOptions.pem_key_cert_pairs.push_back(keyCertPair);
        sslOptions.force_client_auth = true;

        serverCreds = grpc::SslServerCredentials(sslOptions);
        builder.AddListeningPort(config["listenOn"].asString(), serverCreds);
    } else {
        std::cout << "Warning: Started server in insecure mode." << std::endl;
        serverCreds = grpc::InsecureServerCredentials();
        builder.AddListeningPort(config["listenOn"].asString(), serverCreds);
    }

    // Load allowed management clients
    std::unordered_map<std::string, std::string> allowedMgmtClients;
    if (!secrets.isMember("allowedMgmtClients")) {
        std::cerr << "Warning: No management clients set in secrets file. Management capabilities may be limited." << std::endl;
    } else {
        const Json::Value& mgmtClients = secrets["allowedMgmtClients"];
        for (const auto& client : mgmtClients.getMemberNames()) {
            allowedMgmtClients[client] = mgmtClients[client].asString();
        }
    }

    // Database connection setup
    std::shared_ptr<pgConnectorFactory> dbFactory = std::make_shared<pgConnectorFactory>(
        secrets["postgres"]["host"].asString(),
        secrets["postgres"]["database"].asString(),
        secrets["postgres"]["user"].asString(),
        secrets["postgres"]["password"].asString()
    );

    // Create and register service
    auto measurementService = std::make_shared<CMeasurementApiServicer>(dbFactory, allowedMgmtClients);
    builder.RegisterService(measurementService.get());

    // Build and start gRPC server
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    // Start CLI threads
    CLI cli;
    std::thread getLatestMessagesThread(&CLI::getLatestMessages, &cli);
    getLatestMessagesThread.detach();

    std::thread addManualRequestThread;
    if (interactive) {
        addManualRequestThread = std::thread(&CLI::addManualRequest, &cli);
        addManualRequestThread.detach();
    }

    // Start gRPC server and wait for termination
    server->Wait();

    // Join threads if interactive mode is enabled
    if (interactive && addManualRequestThread.joinable()) {
        addManualRequestThread.join();
    }
    if (getLatestMessagesThread.joinable()) {
        getLatestMessagesThread.join();
    }
}

int main(int argc, char* argv[]) {
    // Parse arguments
    cxxopts::Options options("MeasurementServer", "Server for managing measurements via gRPC.");
    options.add_options()
        ("s,secrets", "Path to secrets file", cxxopts::value<std::string>())
        ("c,config", "Path to config file", cxxopts::value<std::string>())
        ("interactive", "Start server in interactive mode", cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);
    std::string secretsFilePath = result["secrets"].as<std::string>();
    std::string configFilePath = result["config"].as<std::string>();
    bool interactive = result["interactive"].as<bool>();

    // Load secrets and config JSON files
    Json::Value secrets;
    Json::Value config;
    std::ifstream secretsFile(secretsFilePath, std::ifstream::binary);
    secretsFile >> secrets;

    std::ifstream configFile(configFilePath, std::ifstream::binary);
    configFile >> config;

    // Start the server
    serve(secrets, config, interactive);

    return 0;
}
