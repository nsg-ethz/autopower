#include "CLI.h"
#include <iostream>
#include <jsoncpp/json/json.h>

void CLI::addManualRequest() {
    ClientManager cm;
    while (true) {
        pbdef::srvRequest rq;

        std::string k;
        while (std::find(cm.getLoggedInClientsList().begin(), cm.getLoggedInClientsList().end(), k) == cm.getLoggedInClientsList().end()) {
            std::cout << "Select the client you want to send a message to. Available are: ";
            for (const auto& client : cm.getLoggedInClientsList()) {
                std::cout << client << " ";
            }
            std::cout << "\nUse client with key: ";
            std::cin >> k;
        }
        
        rq.clientUid = k;

        std::cout << "Available message types are:\n0: INTRODUCE_SERVER\n1: START_MEASUREMENT\n2: STOP_MEASUREMENT\n3: REQUEST_MEASUREMENT_LIST\n4: REQUEST_MEASUREMENT_DATA\n5: REQUEST_MEASUREMENT_STATUS\n6: REQUEST_AVAILABLE_PP_DEVICE\n";
        int mTpe;
        std::cin >> mTpe;

        if (mTpe == 0) {
            rq.msgType = pbdef::srvRequestType::INTRODUCE_SERVER;
        } else if (mTpe == 1) {
            pbdef::srvRequest availableClientsRequest;
            availableClientsRequest.clientUid = k;
            availableClientsRequest.msgType = pbdef::srvRequestType::REQUEST_AVAILABLE_PP_DEVICE;
            int deviceRequestNo = cm.scheduleNewRequestToClient(k, availableClientsRequest);

            std::string smpInterval;
            int uploadinterval;
            std::cout << "Enter sampling interval: ";
            std::cin >> smpInterval;
            std::cout << "Enter upload interval in minutes: ";
            std::cin >> uploadinterval;

            auto clList = cm.getResponseOfRequestNo(k, deviceRequestNo);
            if (clList.msg.empty()) {
                std::cout << "Could not get ppDevice list from client. Please try again later.\n";
                return;
            }

            Json::Value availablePPDevicesJson;
            Json::Reader reader;
            reader.parse(clList.msg, availablePPDevicesJson);

            std::string ppDev;
            for (const auto& dev : availablePPDevicesJson) {
                ppDev = dev["alias"].asString();
                break; // Assume only one device
            }

            cm.setMsmtSettingsOfClient(k, ppDev, smpInterval, uploadinterval);
            rq.msgType = pbdef::srvRequestType::START_MEASUREMENT;
        } else if (mTpe == 2) {
            rq.msgType = pbdef::srvRequestType::STOP_MEASUREMENT;
        } else if (mTpe == 3) {
            rq.msgType = pbdef::srvRequestType::REQUEST_MEASUREMENT_LIST;
        } else if (mTpe == 4) {
            rq.msgType = pbdef::srvRequestType::REQUEST_MEASUREMENT_DATA;
            std::string remtMsmtName;
            std::cout << "Enter shared measurement ID: ";
            std::cin >> remtMsmtName;
            rq.requestBody = remtMsmtName;
        } else if (mTpe == 5) {
            rq.msgType = pbdef::srvRequestType::REQUEST_MEASUREMENT_STATUS;
        } else if (mTpe == 6) {
            rq.msgType = pbdef::srvRequestType::REQUEST_AVAILABLE_PP_DEVICE;
        }

        std::cout << "Response: " << cm.addSyncRequest(k, rq).msg << "\n";
    }
}

void CLI::getLatestMessages() {
    OutCommunicator pm;
    while (true) {
        std::cout << pm.getNextMessageCli() << std::endl;
    }
}
