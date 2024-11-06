#include "pgConnectorFactory.h"
#include <pqxx/pqxx>

pgConnectorFactory::pgConnectorFactory(const std::string& host, const std::string& database, const std::string& user, const std::string& password)
    : host_(host), database_(database), user_(user), password_(password) {}

std::shared_ptr<pqxx::connection> pgConnectorFactory::createConnection() {
    // Create a connection string and open the connection
    std::string connStr = "host=" + host_ + " dbname=" + database_ + " user=" + user_ + " password=" + password_;
    return std::make_shared<pqxx::connection>(connStr);
}
