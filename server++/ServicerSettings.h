#pragma once

#include "PgConnectorFactory.h"
#include <memory>
#include <unordered_map>
struct ServicerSettings {
    std::shared_ptr<PgConnectorFactory>pgFactory;
    std::unordered_map<std::string, std::string> allowedMgmtClients;

};