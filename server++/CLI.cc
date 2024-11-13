#include "CLI.h"
#include "api.pb.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <stdexcept>
#include <vector>
#include <set>

void CLI::addManualRequest() {
    ClientManager cm;
    while (true) {
        // prepare new request
        autopapi::srvRequest rq;

        std::string k;
        std::vector<std::string> clientsList = cm.getLoggedInClientsList();
        while (std::find(clientsList.begin(), clientsList.end(), k) == clientsList.end()) {
            std::cout << "Select the client you want to send a message to. Available are: ";
            for (const auto& client : clientsList) {
                std::cout << client << " ";
            }
            std::cout << "\nUse client with key: ";
            std::cin >> k;
        }
        
        rq.set_clientuid(k);
        int mTpe;
        while (true) {
            std::cout << "Available message types are:\n0: INTRODUCE_SERVER\n1: START_MEASUREMENT\n2: STOP_MEASUREMENT\n3: REQUEST_MEASUREMENT_LIST\n4: REQUEST_MEASUREMENT_DATA\n5: REQUEST_MEASUREMENT_STATUS\n6: REQUEST_AVAILABLE_PP_DEVICE\n";
            std::cin >> mTpe;
            // check that we got back an int and give back an error else
            if(std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "You provided an invalid input." << std::endl;
            } else {
                break;
            }
        }
        if (mTpe == 0) {
            rq.set_msgtype(autopapi::srvRequestType::INTRODUCE_SERVER);
        } else if (mTpe == 1) {
            autopapi::srvRequest availableClientsRequest;
            availableClientsRequest.set_clientuid(k);
            availableClientsRequest.set_msgtype(autopapi::srvRequestType::REQUEST_AVAILABLE_PP_DEVICE);
            int deviceRequestNo = cm.scheduleNewRequestToClient(k, availableClientsRequest);

            std::string smpInterval;
            int uploadinterval;
            std::cout << "Enter sampling interval: ";
            std::cin >> smpInterval;

            while (true) {
                std::cout << "Enter upload interval in minutes: ";
                std::cin >> uploadinterval;
                if(std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "You provided an invalid input." << std::endl;
                } else {
                    break;
                }
            }

            autopapi::clientResponse clList;
            Json::Value availablePPDevicesJson;
            Json::Reader reader;

            try {
              clList = cm.getResponseOfRequestNo(k, deviceRequestNo);
              if (clList.msg().empty()) {
                throw std::runtime_error("ppDevice list from client is empty. Please check the client for available aliasses.");
              }

                if (!reader.parse(clList.msg(), availablePPDevicesJson)) {
                    throw std::runtime_error("Could not parse ppDevice list from client.");
                }
            } catch (std::runtime_error e) {
                std::string error = "Could not get ppDevice list from client. Please try again later.";
                std::cerr << error << " " << e.what() << std::endl;
                return;
            }

            // check for available devices
            std::set<std::string> availablePPDeviceames;
            for (const auto& dev : availablePPDevicesJson) {
                availablePPDevicesJson.insert(dev["alias"].asString());
            }
            
            std::string ppDev = "";

            while(!availablePPDeviceames.contains(ppDev)) { // .contains only exists in C++ 20 or later
                std::cout << "Enter one device from: ";
                for (std::string dev : availablePPDeviceames) {
                    std::cout << dev << " ";
                }

              std::cin >> ppDev;
            }

            cm.setMsmtSettingsOfClient(k, ppDev, smpInterval, uploadinterval);
            rq.set_msgtype(autopapi::srvRequestType::START_MEASUREMENT);
        } else if (mTpe == 2) {
            rq.set_msgtype(autopapi::srvRequestType::STOP_MEASUREMENT);
        } else if (mTpe == 3) {
            rq.set_msgtype(autopapi::srvRequestType::REQUEST_MEASUREMENT_LIST);
        } else if (mTpe == 4) {
            rq.set_msgtype(autopapi::srvRequestType::REQUEST_MEASUREMENT_DATA);
            std::string remtMsmtName;
            std::cout << "Enter shared measurement ID: ";
            std::cin >> remtMsmtName;
            // TODO: Maybe add some checking here too?
            rq.set_requestbody(remtMsmtName);
        } else if (mTpe == 5) {
            rq.set_msgtype(autopapi::srvRequestType::REQUEST_MEASUREMENT_STATUS);
        } else if (mTpe == 6) {
            rq.set_msgtype(autopapi::srvRequestType::REQUEST_AVAILABLE_PP_DEVICE);
        }

        std::cout << "Response: " << cm.addSyncRequest(k, rq).msg() << std::endl; // blocks until the request was fulfilled or timeout
    }
}

void CLI::getLatestMessages() {
    // get the messages from the client manager and cleans up queue of last messages
    OutCommunicator oc;
    while (true) {
        std::cout << oc.getNextMessageCli() << std::endl;
    }
}
