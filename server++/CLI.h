#pragma once

#include "ClientManager.h"
#include "OutCommunicator.h"
#include <string>

class CLI {
public:
    void addManualRequest();
    void getLatestMessages();
};