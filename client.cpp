// client.cpp — build:
//   Linux/macOS:  g++ -std=c++17 -pthread client.cpp -o client
//   MinGW:        g++ -std=c++17 -pthread client.cpp -o client.exe -lws2_32
//   MSVC:         cl /std:c++17 client.cpp ws2_32.lib
// Usage: client [ip] [port]   (defaults to 127.0.0.1:8080)
#include "sock.h"

#include <iostream>
#include <thread>
#include <string>
#include <mutex>
#include <cstdlib>

static sock_t g_sock = SOCK_INVALID;
static std::mutex g_out_mutex;   // protects interleaved stdout writes from two threads

// Receiving thread: blocking recv in a loop
void receive_loop() {
    char buffer[1024];
    while (true) {
        int n = sock_recv(g_sock, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            std::lock_guard<std::mutex> lock(g_out_mutex);
            std::cout << "\n[system] Connection closed\n";
            exit(0);
        }
        buffer[n] = '\0';
        {
            // print what the server sent (already prefixed with [N] by the server),
            // then redraw the input prompt
            std::lock_guard<std::mutex> lock(g_out_mutex);
            std::cout << buffer << "> " << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    if (sock_init() != 0) {
        sock_perror("sock_init");
        return 1;
    }

    std::string server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    // Port: argv[2] or 8080 by default
    unsigned short port = 8080;
    if (argc > 2) {
        char* end = nullptr;
        unsigned long p = std::strtoul(argv[2], &end, 10);
        if (!end || *end != '\0' || p < 1 || p > 65535) {
            std::cerr << "Invalid port: " << argv[2] << "\n";
            return 1;
        }
        port = (unsigned short)p;
    }

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock == SOCK_INVALID) { sock_perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &addr.sin_addr);

    if (connect(g_sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_perror("connect");
        sock_close(g_sock);
        sock_cleanup();
        return 1;
    }
    std::cout << "Connected to " << server_ip << ":" << port << "\n";
    std::cout << "Type messages and press Enter:\n> " << std::flush;

    std::thread receiver(receive_loop);
    receiver.detach();

    // Main thread: read stdin and send
    std::string line;
    while (std::getline(std::cin, line)) {
        line += "\n";
        sock_send(g_sock, line.data(), line.size());
        std::lock_guard<std::mutex> lock(g_out_mutex);
        std::cout << "> " << std::flush;   // redraw the input prompt
    }

    sock_close(g_sock);
    sock_cleanup();
    return 0;
}