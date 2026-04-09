#ifndef DOIP_CLIENT_H   // Header guard start
#define DOIP_CLIENT_H
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <vector>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include "config.h"
#include "doip-config.h"

class doipHandler {

    private:
    int raw_sock_fd, tcp_sock_fd, recv_len;
    struct ifreq if_idx;
    struct sockaddr_ll udp_socket_address;
    struct sockaddr_in6 tcp_socket_address;
    socklen_t addr_len;
    uint8_t raw_packet_buffer[TOTAL_PACKET_LEN], recv_buffer[RECV_BUFFER_LEN];
    uint8_t dest_mac_addr[6] = {0}, src_mac_addr[6] = {0}; 
    char vi_response_src_ip[INET6_ADDRSTRLEN];
    uint16_t vi_response_src_port, ra_response_src_addr, ra_response_target_addr;

    public:
    bool createUdpRawSocket();
    bool bindInterface(Config& config);
    void constructUdpPacket(Config& config);
    uint16_t createPseudoHeader(const uint8_t *buffer, Config& config);
    uint16_t computeCheckSum(const std::vector<uint8_t>& data); // This needs to be reviewed, returns wrong checksum values
    bool sendVehicleIdentificationReq(Config& config);
    bool recvVehicleIdentificationRes(Config& config);
    bool createTcpSocket();   
    bool startTcpConnection(Config& config);
    bool sendRequest(const uint8_t *payload, int length, const char *payload_type);
    bool recvResponse(const char* payload_type, int packet_len, Config& config);
    std::atomic<bool> keep_alive_active{true};
    bool keepAliveTester(Config& config);
    void parseRoutingActivationRes(const uint8_t *buffer, int length, Config& config); // Might needed later
    void printPacket(const uint8_t* buffer, int length);
    void disconnectUdp();
    void disconnectTcp();
};

#endif
/*
Questions:
1. How do we generalize this doip structure?
2. Currently, only raw packet is getting created for the first part, how should we proceed with multiple packets will be sent over UDP or TCP?
3. How to remove the hardcoded part from the raw packet, and create new packets with dynamic values?
4. Is there a way to get the Ethernet header and Vlan tag values dynamically?
5. Why ther's a need to create one raw packet for the Udp?
6. Does udp payload does not get transfer to ECU without creating one raw packet?
7. How to add the tester-alive-thread in the current doip client-side file?
8. How the mqtt payload data will be sent in the tester present packet?

Doubts:
1. Get to know about the Config.ini
2. How the tcp packet will get transferred over the current network

Tasks:
1. Learn how to read values from the config.ini
2. Learn about the Keep alive thread used in the handshake
3. Learn how doip and mqtt threads will be integrated
4. Fix the compute Checksum Function
5. Add one condition for the mqtt subscriber thread to wait for the handshake completion.
6. Test this application with Raspi as client and Windows as Server
7. Create and test the main file without Switch case used
8. Read the static values and macros from the config.ini
9. Implement one method that will close the whole application (topic/close)
*/




