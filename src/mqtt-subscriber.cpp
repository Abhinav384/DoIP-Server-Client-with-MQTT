#include "mqtt-subscriber.h"


// Constructor Calling
mqttHandler::mqttHandler(){
    conn_opts = MQTTClient_connectOptions_initializer;
    return_code = 0;
    handshake_complete = false;
    running = true;
}

// Sets Member Value
void mqttHandler::setter(bool flag){
    {
        std::lock_guard<std::mutex> lock(mtx);
        handshake_complete = flag;
    }
    cv.notify_all();
}

// Returns Member Value
bool mqttHandler::getter(){
    std::lock_guard<std::mutex> lock(mtx);
    return handshake_complete;
}

// Makes thread Wait
void mqttHandler::waitForHandshake() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return handshake_complete; });
}

// Create MQTT client
bool mqttHandler::clientCreation(Config& config) {
    std::string broker_ip = config.getValue("MQTT", "BROKER_IP");
    std::string client_id = config.getValue("MQTT", "CLIENT_ID");
    config.trim(broker_ip);
    config.trim(client_id);
    snprintf(address, sizeof(address), "tcp://%s:1883", broker_ip.c_str());
    std::cout << "Broker Ip: " << broker_ip << std::endl;
    std::cout << "Client Id: " << client_id << std::endl;
    std::cout << "Formatted Address: " << address << std::endl;
    std::cout << "Formatted client-Id: " << client_id << std::endl;
    return_code = MQTTClient_create(&client, address, client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (return_code != MQTTCLIENT_SUCCESS) {
        std::cerr << "Failed to create MQTT client, return code " << return_code << std::endl;
        return false;
    }
    return true;
}

// Connect to MQTT broker
bool mqttHandler::connectClient() {
    MQTTClient_setCallbacks(client, this, connLost, msgArrived, onDeliveryComplete);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    std::cout << "Connecting to Mosquitto MQTT broker..." << std::endl;
    if ((return_code = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        std::cerr << "Failed to connect, return code " << return_code << std::endl;
        return false;
    }
    return true;
}

// Subscribe to topics
bool mqttHandler::subscribe(Config& config) {
    std::string test_topic = config.getValue("MQTT", "TOPIC");
    std::string camera_topic = config.getValue("MQTT", "CAMERA_TOPIC");
    std::string shut_topic = config.getValue("MQTT", "SHUT_TOPIC");
    const int qos = std::stoi(config.getValue("MQTT", "QOS").c_str());
    config.trim(test_topic);
    config.trim(camera_topic);
    config.trim(shut_topic);
    std::cout << "Subscribing to topics: " << test_topic << ", " << camera_topic << ", " << shut_topic << std::endl;
    if ((return_code = MQTTClient_subscribe(client, test_topic.c_str(), qos)) != MQTTCLIENT_SUCCESS ||
        (return_code = MQTTClient_subscribe(client, camera_topic.c_str(), qos)) || 
        (return_code = MQTTClient_subscribe(client, shut_topic.c_str(), qos)) != MQTTCLIENT_SUCCESS) {
        std::cerr << "Failed to subscribe, return code " << return_code << std::endl;
        return false;
    }
    return true;
}

// Socket Creation
bool mqttHandler::socketCreation(Config& config){
    const int dest_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_PORT").c_str());
    std::string dest_ip = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_IP");
    config.trim(dest_ip);
    // Tcp socket creation
    tcp_sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_sock_fd < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return false;
    }
    std::cout << "Socket Creation Successful..." << std::endl;

    // Initialize server address structure
    memset(&tcp_socket_address, 0, sizeof(tcp_socket_address));
    tcp_socket_address.sin6_family = AF_INET6;
    tcp_socket_address.sin6_port = htons(dest_port);
    
    std::cout << "Ipv6 Destination Address: " << dest_ip.c_str() << std::endl;
    std::cout << "Ipv6 Destination Port: " << dest_port << std::endl;

    // Convert IPv6 address from text to binary form
    if (inet_pton(AF_INET6, dest_ip.c_str(), &tcp_socket_address.sin6_addr) <= 0) {
        std::cerr << "Invalid server IPv6 address." << std::endl;
        disconnectTcp();
        return false;
    }
    
    std::cout << "Ipv6 Conversion Successful..." << std::endl;
    
    // Connect to server
    addr_len = sizeof(tcp_socket_address);
    if (connect(tcp_sock_fd, (struct sockaddr*)&tcp_socket_address, addr_len) < 0) {
        std::cerr << "Connection to the server failed." << strerror(errno) << std::endl;
        disconnectTcp();
        return false;
    }
    
    std::cout << "Tcp Connection Successful for Mqtt Subscriber.." << std::endl;

    return true;
}

// Sends Request for different DoIP Paylaods
bool mqttHandler::sendRequest(const uint8_t* payload, int length, const char* payload_type){
    if(send(tcp_sock_fd, payload, length, 0) < 0){
        std::cerr << "Failed to send " << payload_type <<" Req." << std::endl;
        disconnectTcp();
        return false;
    }
    std :: cout << payload_type <<" Req sent Sucessfully..." << std::endl;
    return true;
}

// Extraction of Mode from received message
void mqttHandler::extractMode(const char *json, char *mode) {
    const char *modeStart = strstr(json, "\"mode\"");
    if (modeStart) {
        modeStart = strchr(modeStart, ':');
        if (modeStart) {
            modeStart++;
            sscanf(modeStart, "\"%[^\"]\"", mode);
        }
    }
}

// Extract camera ID from JSON message
void mqttHandler::extractCameraID(const char *json, char *cameraId) {
    const char *cameraIdStart = strstr(json, "\"cameraId\"");
    if (cameraIdStart) {
        cameraIdStart = strchr(cameraIdStart, ':');
        if (cameraIdStart) {
            cameraIdStart++;
            sscanf(cameraIdStart, "\"%[^\"]\"", cameraId);
        }
    }
}

// Callback for message arrival
int mqttHandler::msgArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // Ensure topic Name is Valid
    if (!topicName || !std::isprint(static_cast<unsigned char>(topicName[0]))) {
        std::cerr << "Received corrupted topic name! Ignoring message..." << std::endl;
        return 1;
    }	
    // Creating object instance to access member variables
    mqttHandler* instance = static_cast<mqttHandler*>(context);

    // Shutting down Application
    if(strcmp(topicName, "topic/shut") == 0){
        std::cout << "Message arrived on topic: " << topicName << std::endl;
        std::cout << "Shutdown Request Received, Stopping Application..." << std::endl;
        if(instance){
            instance->closeApplication();   
            instance->disconnect();
            instance->disconnectTcp();
        }
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return EXIT_SUCCESS;
    }

    // Checking for Handshake Completion
    std::cout << "Waiting for Handshake Completion...." << std::endl;
    try {
        instance->waitForHandshake();
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }

    // Closing the threads in wait state
    if (!instance->isRunning()) {
        std::cout << "Application is stopping, ignoring message." << std::endl;
        return 1;
    }

    // DoIP Handshake Completion
    std::cout << "DoIP Handshake Completed Successfully." << std::endl; 
    std::cout << "Message arrived on topic: " << topicName << std::endl;
    std::cout << "Message: " << static_cast<char *>(message->payload) << std::endl;

    // Extract mode and camera ID from message
    char mode[256] = {0};
    char cameraId[256] = {0};
    extractMode(static_cast<char *>(message->payload), mode);
    extractCameraID(static_cast<char *>(message->payload), cameraId);
    std::cout << "Extracted Mode: " << mode << std::endl;
    std::cout << "Extracted Camera ID: " << cameraId << std::endl;
    
    // Selecting payloads based on Camera Selection
    const uint8_t *payload;
    const char *payload_type;
    int payload_len;
    if (strcmp(cameraId, "Front") == 0){
        payload = frontCameraSelectionReq;
        payload_len = frontCameraSelectionReqSize;
        payload_type = FRONT_CAMERA_SELECTION;
    } else if (strcmp(cameraId, "Rear") == 0){
        payload = rearCameraSelectionReq;
        payload_len = rearCameraSelectionReqSize;
        payload_type = REAR_CAMERA_SELECTION;
    } else if (strcmp(cameraId, "Left") == 0){
        payload = leftCameraSelectionReq;
        payload_len = leftCameraSelectionReqSize;
        payload_type = LEFT_CAMERA_SELECTION;
    } else if (strcmp(cameraId, "Right") == 0){
        payload = rightCameraSelectionReq;
        payload_len = rightCameraSelectionReqSize;
        payload_type = RIGHT_CAMERA_SELECTION;
    } else{
        std::cout << "Wrong Camera Selection: " << cameraId << std::endl;
        return EXIT_FAILURE;
    }

    try {
        if(!instance->sendRequest(payload, payload_len, payload_type)){
            return EXIT_FAILURE;
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception caught while Sending Req by Mqtt Subscriber: " << e.what() << std::endl;
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return EXIT_SUCCESS;
}

// Callback for connection lost
void mqttHandler::connLost(void *context, char *cause) {
    std::cerr << "\nConnection lost" << std::endl;
    std::cerr << "Cause: " << cause << std::endl;
}

// Callback for delivery complete
void mqttHandler::onDeliveryComplete(void *context, MQTTClient_deliveryToken dt) {
    std::cout << "Message with token value " << dt << " delivery confirmed" << std::endl;
}

// Disconnect from MQTT broker
void mqttHandler::disconnect() {
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

// Closes Tcp Socket
void mqttHandler::disconnectTcp(){
    close(tcp_sock_fd);
}

// Closes Application
void mqttHandler::closeApplication(){
    running = false;
    cv.notify_all();
}

// Running Flag
bool mqttHandler::isRunning(){
    return running;
}
