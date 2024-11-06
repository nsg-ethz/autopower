#pragma once

#include <json/json.h>
#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <string>

// Forward declarations for other components used in main.cpp
class CLI;
class CMeasurementApiServicer;
class pgConnectorFactory;

void serve(const Json::Value& secrets, const Json::Value& config, bool interactive);
