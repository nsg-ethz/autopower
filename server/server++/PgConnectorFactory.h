#pragma once

#include <string>
#include <memory>
#include <pqxx/pqxx>

class pgConnectorFactory {
public:
    pgConnectorFactory(const std::string& host, const std::string& database, const std::string& user, const std::string& password);

    // Creates a new PostgreSQL connection
    std::shared_ptr<pqxx::connection> createConnection();

private:
    std::string host_;
    std::string database_;
    std::string user_;
    std::string password_;
};