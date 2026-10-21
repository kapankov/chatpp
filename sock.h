// sock.h — thin cross-platform socket wrapper.
// Windows: link ws2_32 (-lws2_32 for g++, ws2_32.lib for MSVC).
// POSIX: works as is.
#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>   // must come first, before windows.h
  #include <ws2tcpip.h>
  #ifdef _MSC_VER
  #pragma comment(lib, "ws2_32.lib")
  #endif
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <cerrno>
#endif

#include <cstdio>
#include <cstddef>

// --- Socket type and "no socket" sentinel ---
#ifdef _WIN32
using sock_t = SOCKET;
constexpr sock_t SOCK_INVALID = INVALID_SOCKET;
#else
using sock_t = int;
constexpr sock_t SOCK_INVALID = -1;
#endif

// Initialize the socket subsystem. 0 = OK. No-op on POSIX.
inline int sock_init() {
#ifdef _WIN32
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc == 0) SetConsoleOutputCP(CP_UTF8); // print UTF-8 correctly in the console
    return rc;
#else
    return 0;
#endif
}

// Shut down the socket subsystem (call once at program exit).
inline void sock_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Close a socket (closesocket/close). Invalid sockets are ignored.
inline void sock_close(sock_t s) {
    if (s == SOCK_INVALID) return;
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

// Last socket error: WSAGetLastError() / errno.
inline int sock_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// Print the last socket error message (perror counterpart).
inline void sock_perror(const char* msg) {
#ifdef _WIN32
    char* buf = nullptr;
    DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, (DWORD)sock_error(), 0, (LPSTR)&buf, 0, nullptr);
    std::fprintf(stderr, "%s: %.*s (code %d)\n", msg,
                 n ? (int)n : 6, n ? buf : "error  ", sock_error());
    if (buf) LocalFree(buf);
#else
    std::perror(msg);
#endif
}

// Send data. Returns bytes sent or -1 on error (on Windows SOCKET_ERROR == -1).
inline int sock_send(sock_t s, const char* buf, size_t len) {
#ifdef _WIN32
    return send(s, buf, (int)len, 0);
#else
    return (int)::send(s, buf, len, 0);
#endif
}

// Receive data. >0 — bytes, 0 — peer closed the connection, -1 — error.
inline int sock_recv(sock_t s, char* buf, size_t len) {
#ifdef _WIN32
    return recv(s, buf, (int)len, 0);
#else
    return (int)::recv(s, buf, len, 0);
#endif
}