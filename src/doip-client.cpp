#include "doip-client.h"

// Creates Udp Raw Socket
bool doipHandler::createUdpRawSocket() {
    raw_sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_sock_fd < 0) {
        std::cerr << "Udp Raw Socket creation Failed, Permission Denied." << std::endl;
        return false;
    }
    return true;
}

// Sets the current Ethernet Interface Index
bool doipHandler::bindInterface(Config& config) {
    std::string interface = config.getValue("UDP-RAW-PACKET-NETWORK", "INTERFACE");
    config.trim(interface);
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, interface.c_str(), IFNAMSIZ - 1);
    if (ioctl(raw_sock_fd, SIOCGIFINDEX, &if_idx) < 0) {
        std::cerr << "Interface indexing failed." << std::endl;
        disconnectUdp();
        return false;
    }

    // Bind socket to interface
    if (setsockopt(raw_sock_fd, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(), interface.length()) < 0){
        std::cerr << "Binding to Interface Failed." << std::endl;
        disconnectUdp();
        return false;
    }
    return true;
}

// Computes Checksum
uint16_t doipHandler::computeCheckSum(const std::vector<uint8_t>& data) {
    uint32_t sum = 0;
    size_t length = data.size();
    const uint8_t* buffer = data.data();
    
    std::cout << "Checksum Computaion Packet: " << std::endl;
    for (uint8_t byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Sum up the 16-bit words
    for (size_t i = 0; i + 1 < length; i += 2) {
        sum += (buffer[i] << 8) | buffer[i + 1];
    }

    // If the length is odd, add the last byte
    if (length % 2 != 0) {
        sum += buffer[length - 1] << 8;
    }

    // Add the carry
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Return the one's complement
    return ~static_cast<uint16_t>(sum);
}

// Creates Pseudo Header
uint16_t doipHandler::createPseudoHeader(const uint8_t* raw_packet, Config& config) {
    const int eth_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "ETH_HDR_LEN").c_str());
    const int vlan_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VLAN_HDR_LEN").c_str());
    const int ip_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "IP_HDR_LEN").c_str());
    const int udp_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "UDP_HDR_LEN").c_str());
    const int vi_payload_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VI_PAYLOAD_LEN").c_str());

    const struct ip6_hdr* ip6 = reinterpret_cast<const struct ip6_hdr*>(raw_packet + eth_hdr_len + vlan_hdr_len);
    const struct udphdr* udp = reinterpret_cast<const struct udphdr*>(raw_packet + eth_hdr_len + vlan_hdr_len + ip_hdr_len);
    const uint8_t* payload = raw_packet + eth_hdr_len + vlan_hdr_len + ip_hdr_len + udp_hdr_len;

    // Create vectors for source IP, destination IP, UDP header, and payload
    std::vector<uint8_t> source_ip(ip6->ip6_src.s6_addr, ip6->ip6_src.s6_addr + 16);
    std::vector<uint8_t> dest_ip(ip6->ip6_dst.s6_addr, ip6->ip6_dst.s6_addr + 16);
    std::vector<uint8_t> udp_header(reinterpret_cast<const uint8_t*>(udp), reinterpret_cast<const uint8_t*>(udp) + udp_hdr_len);
    std::vector<uint8_t> udp_payload(payload, payload + vi_payload_len);

    // Create pseudo header vector
    std::vector<uint8_t> pseudo_header;
    pseudo_header.insert(pseudo_header.end(), source_ip.begin(), source_ip.end()); 
    pseudo_header.insert(pseudo_header.end(), dest_ip.begin(), dest_ip.end());     
    pseudo_header.push_back(0x00);                                              
    pseudo_header.push_back(0x11);                                       
    uint16_t udp_length = udp_hdr_len + vi_payload_len;                     
    pseudo_header.push_back((udp_length >> 8) & 0xFF);                         
    pseudo_header.push_back(udp_length & 0xFF);                                   

    // Print pseudo header
    std::cout << "Pseudo Header: ";
    for (uint8_t byte : pseudo_header) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Combine pseudo header, UDP header, and payload for checksum calculation
    std::vector<uint8_t> checksum_data;
    checksum_data.insert(checksum_data.end(), pseudo_header.begin(), pseudo_header.end());
    checksum_data.insert(checksum_data.end(), udp_header.begin(), udp_header.end());
    checksum_data.insert(checksum_data.end(), udp_payload.begin(), udp_payload.end());

    // Compute checksum
    static uint16_t checksum = computeCheckSum(checksum_data);
    std::cout << "Calculated Checksum: 0x" << std::hex << std::setw(4) << std::setfill('0') << checksum << std::endl;
    return checksum;
}

// Contructs Udp Packet
void doipHandler:: constructUdpPacket(Config& config){
    std::string dest_mac = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_MAC");
    std::string src_mac = config.getValue("UDP-RAW-PACKET-NETWORK", "SRC_MAC");
    std::string src_ip = config.getValue("UDP-RAW-PACKET-NETWORK", "SRC_IP");
    std::string dest_multicast_ip = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_MULTICAST_IP");
    const int udp_req_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "UDP_REQ_PORT").c_str());
    const int tcp_req_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "TCP_REQ_PORT").c_str());
    const int dest_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_PORT").c_str());
    const int vlan_tag_id = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VLAN_TAG_ID").c_str());
    const int eth_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "ETH_HDR_LEN").c_str());
    const int vlan_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VLAN_HDR_LEN").c_str());
    const int ip_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "IP_HDR_LEN").c_str());
    const int udp_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "UDP_HDR_LEN").c_str());
    const int vi_payload_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VI_PAYLOAD_LEN").c_str());
    const int total_packet_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "TOTAL_PACKET_LEN").c_str());
    config.parseMac(dest_mac, dest_mac_addr);
    config.parseMac(src_mac, src_mac_addr);
    config.trim(src_ip);
    config.trim(dest_multicast_ip);
    // Ethernet Header
    memcpy(raw_packet_buffer, dest_mac_addr, 6);      
    memcpy(raw_packet_buffer + 6, src_mac_addr, 6);   
    raw_packet_buffer[12] = 0x81;             
    raw_packet_buffer[13] = 0x00;

    // VLAN Tag
    raw_packet_buffer[14] = (0x00 | ((vlan_tag_id >> 8) & 0x0F)); 
    raw_packet_buffer[15] = vlan_tag_id & 0xFF;                 
    raw_packet_buffer[16] = 0x86;                           
    raw_packet_buffer[17] = 0xDD;

    // IPv6 Header
    struct ip6_hdr *ip6 = (struct ip6_hdr *)(raw_packet_buffer + eth_hdr_len + vlan_hdr_len);
    memset(ip6, 0, ip_hdr_len);
    ip6->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);  // Naming changed from ip6->ip6_flow to ip6->ip6_vcf
    ip6->ip6_plen = htons(udp_hdr_len + vi_payload_len);  
    ip6->ip6_nxt = IPPROTO_UDP;                       
    ip6->ip6_hlim = 1;                              
    inet_pton(AF_INET6, src_ip.c_str(), &ip6->ip6_src);      
    inet_pton(AF_INET6, dest_multicast_ip.c_str(), &ip6->ip6_dst);   

    // UDP Header
    struct udphdr *udp = (struct udphdr *)(raw_packet_buffer + eth_hdr_len + vlan_hdr_len + ip_hdr_len);
    udp->uh_sport = htons(udp_req_port);                
    udp->uh_dport = htons(dest_port);                
    udp->uh_ulen = htons(udp_hdr_len + vi_payload_len); 
    udp->uh_sum = 0;                                

    // Creating the Payload
    uint8_t *payload = raw_packet_buffer + eth_hdr_len + vlan_hdr_len + ip_hdr_len + udp_hdr_len;
    memcpy(payload, viReq, vi_payload_len);

    // Calculating Checksum
    udp->uh_sum = htons(createPseudoHeader(raw_packet_buffer, config));
    printPacket(raw_packet_buffer, total_packet_len);
}

// Sends Vehicle Identification Request
bool doipHandler::sendVehicleIdentificationReq(Config& config) {
    std::string dest_mac = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_MAC");
    const int total_packet_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "TOTAL_PACKET_LEN").c_str());
    config.trim(dest_mac);

    // Setting Destination Address
    memset(&udp_socket_address, 0, sizeof(struct sockaddr_ll));
    udp_socket_address.sll_ifindex = if_idx.ifr_ifindex;
    udp_socket_address.sll_halen = ETH_ALEN;
    memcpy(udp_socket_address.sll_addr, dest_mac.c_str(), 6);

    //Sending Packet
    if (sendto(raw_sock_fd, raw_packet_buffer, total_packet_len, 0, (struct sockaddr *)&udp_socket_address, sizeof(struct sockaddr_ll)) < 0) {
        std::cerr << "Failed to send Vehicle Identification Req" << std::endl;
        disconnectUdp();
        return false;
    }
    std::cout << "Vehicle Identification Req sent successfully..." << std::endl;
    return true;
}

// Receives Vehicle Identification Response
bool doipHandler::recvVehicleIdentificationRes(Config& config) {
    const int recv_buffer_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "RECV_BUFFER_LEN").c_str());
    const int vi_res_len = std::stoi(config.getValue("TCP-RESPONSE-PAYLOADS-LENGTH", "VI_RES_LEN").c_str());
    const int eth_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "ETH_HDR_LEN").c_str());
    const int vlan_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VLAN_HDR_LEN").c_str());
    const int ip_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "IP_HDR_LEN").c_str());
    const int tcp_req_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "TCP_REQ_PORT").c_str());
    std::string dest_ip = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_IP");
    config.trim(dest_ip);

    // Keep on listening for packets
    while (true) {
        // Packet Receiving
        addr_len = sizeof(struct sockaddr_ll);
        recv_len = recvfrom(raw_sock_fd, recv_buffer, recv_buffer_len, 0,
                    (struct sockaddr *)&udp_socket_address, &addr_len);
        if (recv_len < 0) {
            std::cerr << "Failed to receive packet." << std::endl;
            return false;
        }

        // Checking for correct Response Packet
        if(recv_len == vi_res_len){
            std::cout << "Vehicle Identification Response Received..." << std::endl;

            // Printing Vehicle Indentification Response
            printPacket(recv_buffer, recv_len);

            // Getting Identified Ecu Ip
            struct ip6_hdr *ipv6_hdr = (struct ip6_hdr *)(recv_buffer + eth_hdr_len + vlan_hdr_len);
            if (inet_ntop(AF_INET6, &ipv6_hdr->ip6_src, vi_response_src_ip, sizeof(vi_response_src_ip))) {
                if(vi_response_src_ip == dest_ip){
                    std::cout << "Valid packet received from Ip: " << vi_response_src_ip << std::endl;
                }
            }

            // Getting Identified Ecu Port
            struct udphdr *udp_hdr = (struct udphdr *)(recv_buffer + eth_hdr_len + vlan_hdr_len + ip_hdr_len);
            vi_response_src_port = ntohs(udp_hdr->uh_sport);
            if (vi_response_src_port == tcp_req_port) {
                std::cout << "Valid Packet received from Port: " << vi_response_src_port << std::endl;
                break;
            } else {
                std::cout << "Discarded packet." << std::endl;
            }
        }
    }
    return true;
}

// Creates Tcp Socket
bool doipHandler::createTcpSocket(){
     // Tcp socket creation
    tcp_sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_sock_fd < 0) {
        std::cerr << "Socket creation failed." << std::endl;
        return false;
    }
    std::cout << "Socket Creation Successful..." << std::endl;
    return true;
}

// Performs Tcp Connection
bool doipHandler::startTcpConnection(Config& config){
    const int dest_port = std::stoi(config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_PORT").c_str());
    std::string dest_ip = config.getValue("UDP-RAW-PACKET-NETWORK", "DEST_IP");
    config.trim(dest_ip);
    // Initialize server address structure
    memset(&tcp_socket_address, 0, sizeof(tcp_socket_address));
    tcp_socket_address.sin6_family = AF_INET6;
    tcp_socket_address.sin6_port = htons(dest_port);
    
    std::cout << "Ipv6 Destination Address: " << dest_ip << std::endl;
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
    
    std::cout << "Tcp Connection Successful for DoIP Client.." << std::endl;

    return true;
}

// Sends Request for different DoIP Paylaods
bool doipHandler::sendRequest(const uint8_t* payload, int length, const char* payload_type){
    if(send(tcp_sock_fd, payload, length, 0) < 0){
        std::cerr << "Failed to send " << payload_type <<" Req." << std::endl;
        disconnectTcp();
        return false;
    }
    std :: cout << payload_type <<" Req sent Sucessfully..." << std::endl;
    return true;
}

// Receives Response for different DoIP Payloads
bool doipHandler::recvResponse(const char* payload_type, int packet_len, Config& config){
    const int recv_buffer_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "RECV_BUFFER_LEN").c_str());

    recv_len = recv(tcp_sock_fd, recv_buffer, recv_buffer_len, 0);
    if (recv_len < 0) {
        std::cerr << "Failed to receive " << payload_type <<" Response." << std::endl;
        return false;
    }

    // Checking for correct Response Packet
    if(recv_len == packet_len){
        std::cout << payload_type << " Response Received..." << std::endl;
        //Printing Routing Activation Response
        printPacket(recv_buffer, recv_len);
    }
    return true;
}

// Keep-Alive Tester Present Thread
bool doipHandler::keepAliveTester(Config& config){
    const int keep_alive_interval = std::stoi(config.getValue("TCP-RESPONSE-PAYLOADS-LENGTH", "KEEP_ALIVE_INTERVAL").c_str());

    while (keep_alive_active) {
        try {
            // Send Tester Present message
            if(!sendRequest(diagTesterPresentReq, diagTesterPresentReqSize, DIAG_TESTER_PRESENT)){
                std::cerr << "Error: " << strerror(errno) << std::endl;
                break;
            }
            // Wait for the keep-alive interval
            std::this_thread::sleep_for(std::chrono::milliseconds(keep_alive_interval));
        } catch (const std::exception& e) {
            std::cerr << "Exception in Keep-Alive Thread: " << e.what() << std::endl;
            break;
        }
    }
    std::cout << "Keep-Alive Thread stopped." << std::endl;
    return true;
}

// Parses Routing Activation Response --> To Extract the Logical Source Address and Target Address
void doipHandler::parseRoutingActivationRes(const uint8_t* buffer, int length, Config& config){
    const int eth_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "ETH_HDR_LEN").c_str());
    const int vlan_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "VLAN_HDR_LEN").c_str());
    const int ip_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "IP_HDR_LEN").c_str());
    const int tcp_hdr_len = std::stoi(config.getValue("UDP-RAW-PAKCET-FIELDS", "TCP_HDR_LEN").c_str());

    const uint8_t* ra_response_ptr = buffer + eth_hdr_len + vlan_hdr_len + ip_hdr_len + tcp_hdr_len;
    ra_response_src_addr = (ra_response_ptr[86] << 8) | ra_response_ptr[87];
    ra_response_target_addr = (ra_response_ptr[88] << 8) | ra_response_ptr[89];
}

// Prints packet
void doipHandler::printPacket(const uint8_t* buffer, int length) {
    std::cout << "Packet of length " << length << ":\n";
    // Prints raw content of the packet
    for (int i = 0; i < length; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(buffer[i]) << " ";

        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Closes Udp Socket
void doipHandler::disconnectUdp(){
    close(raw_sock_fd);
}

// Closes Tcp Socket
void doipHandler::disconnectTcp(){
    close(tcp_sock_fd);
}


