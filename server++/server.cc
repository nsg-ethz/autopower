#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <jsoncpp/json/json.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <thread>
#include "CLI.h"
#include "CMeasurementApiServicer.h"
#include "PgConnectorFactory.h"
#include "ServicerSettings.h"
#include "server.h"


std::string readFileToString(const std::string &filename) { // source: https://github.com/grpc/grpc/issues/9593
  // read content of cert
  std::string data;
  std::ifstream file(filename.c_str(), std::ios::in);

  if (file.is_open()) {
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    data = ss.str();
  }

  return data;
}

void serve(const Json::Value& secrets, const Json::Value& config, bool interactive) {
    grpc::ServerBuilder builder;
    // SSL setup
    if (secrets.isMember("ssl")) {
        // Load private key
        if (!secrets["ssl"].isMember("privKeyPath")) {
            std::cerr << "ERROR: privKeyPath is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::string privKey = readFileToString(secrets["ssl"]["privKeyPath"].asString());

        // Load certificate chain
        if (!secrets["ssl"].isMember("pubKeyPath")) {
            std::cerr << "ERROR: pubKeyPath is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::string certChain = readFileToString(secrets["ssl"]["pubKeyPath"].asString());

        // Load CA certificate
        if (!secrets["ssl"].isMember("pubKeyCA")) {
            std::cerr << "ERROR: pubKeyCA is not set in config file. Please set it correctly." << std::endl;
            exit(1);
        }
        std::string clientCa = readFileToString(secrets["ssl"]["pubKeyCA"].asString());

        // Set up secure credentials
        grpc::SslServerCredentialsOptions::PemKeyCertPair keyCertPair = {privKey, certChain};
        grpc::SslServerCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = clientCa;
        sslOptions.pem_key_cert_pairs.push_back(keyCertPair);
        sslOptions.force_client_auth = true;

        auto serverCreds = grpc::SslServerCredentials(sslOptions);
        builder.AddListeningPort(config["listenOn"].asString(), serverCreds);
    } else {
        std::cout << "Warning: Started server in insecure mode." << std::endl;
        auto serverCreds = grpc::InsecureServerCredentials();
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
    std::shared_ptr<PgConnectorFactory> dbFactory = std::make_shared<PgConnectorFactory>(
        secrets["postgres"]["host"].asString(),
        secrets["postgres"]["database"].asString(),
        secrets["postgres"]["user"].asString(),
        secrets["postgres"]["password"].asString()
    );

    // Create and register service
    ServicerSettings set = {dbFactory, allowedMgmtClients};
    CMeasurementApiServicer measurementService = CMeasurementApiServicer(&set);
    builder.RegisterService(&measurementService);

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
    std::string secretsFilePath = "";
    std::string configFilePath = "";
    bool interactiveEnabled = false;

    // get arguments from CLI

    int arg;
    while ((arg = getopt(argc, argv, "s:c:ih")) != -1) {
      switch (arg) {
        case 's':
        // secrets file path
        secretsFilePath = optarg;
        break;
        case 'c':
        // config file path
        configFilePath = optarg;
        break;
        case 'i':
        // in interactive mode
        interactiveEnabled = true;
        break;
        case 'h':
        std::cerr << "Autopower server. Available flags:" << std::endl
                  << "Use -s to specify secrets file" << std::endl
                  << "Use -c to specify config file" << std::endl
                  << "  -i Start server in interactive mode to allow managing the CLI directly" << std::endl
                  << "  -h print this help information" << std::endl;

        return 0;
        break;
        default:
        std::cerr << "Available flags: s,c,i,h. Please see -h for explanation" << std::endl;
        return -1;
      }
    }

    if (secretsFilePath.empty()) {
        std::cerr << "No secrets file given. Please use the -s flag to specify a file" << std::endl;
        return -1;
    }

    // load secrets file
    std::ifstream secretsFile(secretsFilePath, std::ifstream::binary);
    if (!secretsFile) {
        std::cerr << "Could not open secrets file. Please check if the file exists and permissons." << std::endl;
        return -1;
    }

    Json::Value secrets;
    secretsFile >> secrets;
    if (!secrets["postgres"]) { // database connection entry must exist
        std::cerr << "Secrets file does not contain postgresString for connecting to database. Please check the file format!" << std::endl;
        return -1;
    }

    Json::Value config;
    if (!configFilePath.empty()) {
    std::ifstream configFile(configFilePath, std::ifstream::binary);
        if (!configFile) {
        std::cerr << "Could not open config file. Please check if the file exists and permissons." << std::endl;
        return -1;
        }
        configFile >> config;
    }



    // Start the server
    serve(secrets, config, interactiveEnabled);

    return 0;
}
