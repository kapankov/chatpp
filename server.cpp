// server.cpp — simple chat server (relays lines to all other clients).
// Build:
//   Linux/macOS:  g++ -std=c++17 -pthread server.cpp -o server
//   MinGW:        g++ -std=c++17 -pthread server.cpp -o server.exe -lws2_32
//   MSVC:         cl /std:c++17 server.cpp ws2_32.lib
// Usage: server [port]   (defaults to 8080)
#include "sock.h"

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <cstdlib>

constexpr unsigned short PORT = 8080;

static std::vector<sock_t> g_clients;
static std::mutex g_clients_mutex;

// Send a line to every client except excluded (if given)
void broadcast(const std::string& msg, sock_t excluded = SOCK_INVALID) {
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    for (sock_t c : g_clients) {
        if (c == excluded) continue;
        sock_send(c, msg.data(), msg.size());
    }
}

void client_thread(sock_t client) {
    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        g_clients.push_back(client);
    }

    char buffer[1024];
    std::string pending;
    int n;
    while ((n = sock_recv(client, buffer, sizeof(buffer))) > 0) {
        pending.append(buffer, n);
        // Split the buffer into lines (clients send '\n'-terminated messages)
        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos + 1);
            pending.erase(0, pos + 1);
            // Prefix the line with the sender's socket number: [N] message
            std::string tagged = "[" + std::to_string(client) + "] " + line;
            broadcast(tagged, client);   // send to everyone except the sender
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_clients_mutex);
        g_clients.erase(std::remove(g_clients.begin(), g_clients.end(), client),
                        g_clients.end());
        std::cout << "[system] client disconnected, active: " << g_clients.size() << "\n";
    }
    sock_close(client);
}

int main(int argc, char* argv[]) {
    // Port: argv[1] or 8080 by default
    unsigned short port = PORT;
    if (argc > 1) {
        char* end = nullptr;
        unsigned long p = std::strtoul(argv[1], &end, 10);
        if (!end || *end != '\0' || p < 1 || p > 65535) {
            std::cerr << "Invalid port: " << argv[1] << "\n";
            return 1;
        }
        port = (unsigned short)p;
    }

    if (sock_init() != 0) {
        sock_perror("sock_init");
        return 1;
    }

    sock_t listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == SOCK_INVALID) { sock_perror("socket"); return 1; }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_perror("bind");
        sock_close(listener);
        sock_cleanup();
        return 1;
    }
    if (listen(listener, 16) != 0) {
        sock_perror("listen");
        sock_close(listener);
        sock_cleanup();
        return 1;
    }
    std::cout << "Server listening on port " << port << "\n";

    while (true) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int clilen = sizeof(client_addr);
#else
        socklen_t clilen = sizeof(client_addr);
#endif
        sock_t client = accept(listener, (sockaddr*)&client_addr, &clilen);
        if (client == SOCK_INVALID) {
            sock_perror("accept");
            continue;
        }
        std::cout << "[system] new client connected\n";
        std::thread(client_thread, client).detach();
    }

    // Unreachable, but for completeness:
    sock_close(listener);
    sock_cleanup();
    return 0;
}