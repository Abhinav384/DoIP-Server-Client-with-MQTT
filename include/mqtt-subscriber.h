#ifndef MQTT_SUBSCRIBER_H   // Header guard start
#define MQTT_SUBSCRIBER_H

#include <iostream>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "config.h"
#include "doip-config.h"

// Use C-style linkage for the Paho MQTT C library
#ifdef __cplusplus
extern "C" {
#endif
#include <MQTTClient.h>
#ifdef __cplusplus
}
#endif

class mqttHandler{

    private:
    MQTTClient client;
    MQTTClient_connectOptions conn_opts;
    char address[256];
    int return_code, tcp_sock_fd;
    bool handshake_complete;
    std::atomic<bool> running;
    std::mutex mtx;
    std::condition_variable cv;
    struct sockaddr_in6 tcp_socket_address;
    socklen_t addr_len;

    public:
    mqttHandler();
    void setter(bool flag);
    bool getter();
    void waitForHandshake();
    bool clientCreation(Config& config);
    bool connectClient();
    bool subscribe(Config& config);
    bool socketCreation(Config& config);
    bool sendRequest(const uint8_t *payload, int length, const char *payload_type);
    static void extractMode(const char *json, char *mode);
    static void extractCameraID(const char *json, char *cameraId);
    static int msgArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
    static void connLost(void *context, char *cause);
    static void onDeliveryComplete(void *context, MQTTClient_deliveryToken dt);
    void disconnect();
    void disconnectTcp();
    void closeApplication();
    bool isRunning();
};

#endif
