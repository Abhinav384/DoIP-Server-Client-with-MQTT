#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 13400
#define BUFFER_SIZE 1024

void print_hex(const char *buffer, ssize_t len) {
    for (ssize_t i = 0; i < len; i++) {
        printf("%02X ", static_cast<unsigned char>(buffer[i]));
    }
    printf("\n");
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE] = {0};

    while (true) {
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }
        buffer[bytesReceived] = '\0';
        print_hex(buffer, bytesReceived);
    }
    
    close(clientSocket);
}

int main() {
    int serverSocket;
    struct sockaddr_in6 serverAddr{};

    // Create socket
    serverSocket = socket(AF_INET6, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return EXIT_FAILURE;
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("SO_REUSEADDR failed");
        close(serverSocket);
        return EXIT_FAILURE;
    }

    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_loopback; // IPv6 localhost (::1)
    serverAddr.sin6_port = htons(PORT);

    // Bind socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Bind failed." << std::endl;
        close(serverSocket);
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Listen failed." << std::endl;
        close(serverSocket);
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on [::1]:" << PORT << std::endl;

    std::vector<std::thread> threads;

    while (true) {
        struct sockaddr_in6 clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket == -1) {
            std::cerr << "Accept failed." << std::endl;
            continue;
        }

        std::cout << "New client connected." << std::endl;

        // Create a new thread for handling this client
        threads.emplace_back(handleClient, clientSocket);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    close(serverSocket);
    return 0;
}