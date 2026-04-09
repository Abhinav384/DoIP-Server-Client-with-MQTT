#include "mqtt-subscriber.h"
#include "doip-client.h"
#include "config.h"

void initializeConfig(const std::string& filename);

void mqttTasks(mqttHandler &subscriber, Config &config){

    // Creating Subscriber Client
    if(!subscriber.clientCreation(config)) return;

    // Making Connection
    if(!subscriber.connectClient()) return;

    // Subscribing to Topic
    if(!subscriber.subscribe(config)){
        subscriber.disconnect(); 
        return;
    }
    
    // Creating Tcp Socket
    if(!subscriber.socketCreation(config)){
        subscriber.disconnect();
        return;
    } 
    
}

void doipTasks(doipHandler &client, mqttHandler &subscriber, Config &config) {
    // Keep-Alive Thread
    std::thread keep_alive_thread;
    // Udp Req Packet Phase
    // Udp Raw Packet Phase
    if (!client.createUdpRawSocket() || !client.bindInterface(config)) {
        std::cerr << "Error: Failed to set up UDP socket.\n";
        return;
    }

    client.constructUdpPacket(config);

    if (!client.sendVehicleIdentificationReq(config)) {
        std::cerr << "Error: Failed to send vehicle identification request.\n";
        return;
    }

    // Tcp Handshake Phase
    if (!client.createTcpSocket()) {
        std::cerr << "Error: Failed to create Socket.\n";
        return;
    }
    
    if (!client.startTcpConnection(config)) {
        std::cerr << "Error: Failed to establish TCP connection.\n";
        return;
    }	

    if (!client.sendRequest(raReq, raReqSize, RA_REQ)) {
        std::cerr << "Error: Failed to send routing activation request.\n";
        return;
    }

    if (!client.sendRequest(diagSessCtrlReq, diagSessCtrlReqSize, DIAG_SESS_CTRl)) {
        std::cerr << "Error: Failed to send diagnostics session request.\n";
        return;
    }
    
    subscriber.setter(true);

    client.keep_alive_active = true;
    keep_alive_thread = std::thread([&]() { client.keepAliveTester(config); });

    // Wait for shutdown signal
    while (subscriber.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Stop the keep-alive thread gracefully
    client.keep_alive_active = false;
    if (keep_alive_thread.joinable()) {
        keep_alive_thread.join();
    }

    // Close all the sockets
    client.disconnectUdp();
    client.disconnectTcp();
    std::cout << "Application Closed." << std::endl;
    return;
}


int main(){
    try {
    // Initialize Client & Subscriber
    doipHandler client;
    mqttHandler subscriber;
    initializeConfig("settings/config.ini");
    Config& config = Config::getInstance();

    // Process Phase
    mqttTasks(subscriber, config);
    doipTasks(client, subscriber, config);
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
    }
    return EXIT_SUCCESS;
}


