#include "PgConnectorFactory.h"
#include <pqxx/pqxx>
#include <string>

PgConnectorFactory::PgConnectorFactory(const std::string& host, const std::string& database, const std::string& user, const std::string& password)
    : host_(host), database_(database), user_(user), password_(password) {}

std::unique_ptr<pqxx::connection> PgConnectorFactory::createConnection() {
    // TODO: Maybe rewrite this or related functions to follow a connection pool instead.
    // Create a connection string and open the connection
    std::string connStr = "host=" + host_ + " dbname=" + database_ + " user=" + user_ + " password=" + password_;
    return std::make_unique<pqxx::connection>(connStr);
}
