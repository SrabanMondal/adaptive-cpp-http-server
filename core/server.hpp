#include<winsock2.h>
#include<mswsock.h>
#include<windows.h>
#include<string>
#include<atomic>
#include<Route.hpp>
#include<chrono>
#include "adaptivelimit.hpp"

struct ConnectionContext {
    int requestCount = 0;
    std::chrono::steady_clock::time_point windowStart;
    HANDLE hTimer;
    SOCKET sock;
    std::string headerBuffer;
    std::vector<char> bodyBuffer;
    size_t expectedBody = 0;
    std::string contentType;
    //AdaptiveState adaptive;
    std::string peerIp;
    bool headerComplete = false;
    ConnectionContext() noexcept = default;
    ~ConnectionContext() noexcept {
        if (hTimer) {
            DeleteTimerQueueTimer(NULL, hTimer, INVALID_HANDLE_VALUE);
            hTimer = NULL;
        }
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }
    ConnectionContext(const ConnectionContext&) = delete; 
    ConnectionContext& operator=(const ConnectionContext&) = delete; 
    ConnectionContext(ConnectionContext&& other) noexcept 
        : sock(other.sock),
        headerBuffer(std::move(other.headerBuffer)),
        bodyBuffer(std::move(other.bodyBuffer)),
        expectedBody(other.expectedBody),
        headerComplete(other.headerComplete),
        windowStart(other.windowStart),
        requestCount(other.requestCount),
        hTimer(other.hTimer)
    {
        other.sock = INVALID_SOCKET;
        other.hTimer = NULL;
    }

    ConnectionContext& operator=(ConnectionContext&& other) noexcept {
        if (this != &other) {
            if (hTimer) {
                DeleteTimerQueueTimer(NULL, hTimer, INVALID_HANDLE_VALUE);
                hTimer = NULL;
            }
            if (sock != INVALID_SOCKET) {
                closesocket(sock);
                sock = INVALID_SOCKET;
            }

            sock = other.sock;
            hTimer = other.hTimer;
            headerBuffer = std::move(other.headerBuffer);
            bodyBuffer = std::move(other.bodyBuffer);
            expectedBody = other.expectedBody;
            headerComplete = other.headerComplete;
            windowStart = other.windowStart;
            requestCount = other.requestCount;
            contentType = std::move(other.contentType);
            other.sock = INVALID_SOCKET;
            other.hTimer = NULL;
        }
        return *this;
    }

};

struct PER_IO_OPERATION_DATA {
    WSAOVERLAPPED overlapped;
    WSABUF buffer;
    char data[1024];
    DWORD bytesRecv;
    ConnectionContext* connCtx;
};

class Server{
    WSAData wsa;
    sockaddr_in serverAddr;
    std::atomic<int> activeClients{0};
    IpRateLimiter ipLimiter; 
    Router& router;
    int MAX_CLIENTS=10000;
    void workerThread(HANDLE hiocp);
    public:
        SOCKET serverSocket;
        Server(int port, Router& r) : router(r){
            WSAStartup(MAKEWORD(2,2),&wsa);
            serverSocket = socket(AF_INET, SOCK_STREAM, 0);
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_family = AF_INET;
        }
        void bindAndListen(int backlog=SOMAXCONN){
            bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
            listen(serverSocket, backlog);
        }

        void start(int threads=-1, int timeout=30000);

};
