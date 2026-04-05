#include "server.hpp"
#include<iostream>
#include <vector>
#include <thread>
#include<mutex>
#include <ws2tcpip.h>
#include "logging.hpp"
using namespace std;

// Build complete response buffer (headers + body) into ctx.sendBuffer
static void prepareSendBuffer(ConnectionContext& ctx, const HttpResponse& res) {
    std::string hdrs = res.headers();
    ctx.sendBuffer.clear();
    ctx.sendBuffer.reserve(hdrs.size() + res.body.size());
    ctx.sendBuffer.insert(ctx.sendBuffer.end(), hdrs.begin(), hdrs.end());
    ctx.sendBuffer.insert(ctx.sendBuffer.end(), res.body.begin(), res.body.end());
    ctx.sendOffset = 0;
}

// Post an async WSARecv on the given socket using the ioData structure
static void postRecv(SOCKET sock, PER_IO_OPERATION_DATA* ioData) {
    ZeroMemory(&ioData->overlapped, sizeof(WSAOVERLAPPED));
    ioData->wsabuf.buf = ioData->data;
    ioData->wsabuf.len = sizeof(ioData->data);
    ioData->opType = IoOpType::RECV;
    DWORD flags = 0;
    int wsares = WSARecv(sock, &ioData->wsabuf, 1, NULL, &flags, &ioData->overlapped, NULL);
    if (wsares == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        // Will be handled on next completion as error
    }
}

// Initiate async WSASend for remaining data in ctx.sendBuffer
static void postSend(SOCKET sock, PER_IO_OPERATION_DATA* ioData, ConnectionContext& ctx) {
    DWORD bytesToSend = static_cast<DWORD>(ctx.sendBuffer.size() - ctx.sendOffset);
    ioData->wsabuf.buf = ctx.sendBuffer.data() + ctx.sendOffset;
    ioData->wsabuf.len = bytesToSend;
    ioData->opType = IoOpType::SEND;
    ioData->bytesTransferred = 0;
    ZeroMemory(&ioData->overlapped, sizeof(WSAOVERLAPPED));
    
    DWORD bytesSent = 0;
    int wsares = WSASend(sock, &ioData->wsabuf, 1, &bytesSent, 0, &ioData->overlapped, NULL);
    if (wsares == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        // Send error — connection will be cleaned up on next completion
    }
}

// Cleanup connection resources
static void cleanupConnection(ConnectionContext* connCtx, PER_IO_OPERATION_DATA* ioData) {
    if (connCtx->hTimer) {
        DeleteTimerQueueTimer(NULL, connCtx->hTimer, INVALID_HANDLE_VALUE);
        connCtx->hTimer = NULL;
    }
    if (connCtx->sock != INVALID_SOCKET) {
        closesocket(connCtx->sock);
        connCtx->sock = INVALID_SOCKET;
    }
    delete connCtx;
    delete ioData;
}

void Server::workerThread(HANDLE hiocp){
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    PER_IO_OPERATION_DATA* ioData;

    while(true){
        BOOL ok = GetQueuedCompletionStatus(hiocp, &bytesTransferred, &completionKey, (LPOVERLAPPED*)&ioData, INFINITE);
        SOCKET clientSocket = (SOCKET)completionKey;
        
        // Handle disconnected client
        if(!ok || bytesTransferred==0){
            if (ioData && ioData->connCtx) {
                activeClients--;
                cleanupConnection(ioData->connCtx, ioData);
            }
            continue;
        }

        auto& ctx = *ioData->connCtx;

        // === SEND COMPLETION ===
        if (ioData->opType == IoOpType::SEND) {
            ctx.sendOffset += bytesTransferred;
            
            if (ctx.sendOffset < ctx.sendBuffer.size()) {
                // Partial send — post remaining data
                postSend(clientSocket, ioData, ctx);
            } else {
                // Send complete — reset connection state and post recv
                ctx.headerBuffer.clear();
                ctx.bodyBuffer.clear();
                ctx.expectedBody = 0;
                ctx.headerComplete = false;
                ctx.sendBuffer.clear();
                ctx.sendOffset = 0;
                postRecv(clientSocket, ioData);
            }
            continue;
        }

        // === RECV COMPLETION ===
        auto now = std::chrono::steady_clock::now();

        bool allowed = true;
        allowed = ipLimiter.allowRequest(ctx.peerIp);
        if (!allowed) {
            logerr("Adaptive rate limit exceeded, sending 429");
            std::string json = R"({"message":"Rate Limit Exceeded. Try again later."})";
            HttpResponse res = JsonResponse(json, 429);
            prepareSendBuffer(ctx, res);
            postSend(clientSocket, ioData, ctx);
            continue;
        }

        if (ctx.hTimer) {
            DeleteTimerQueueTimer(NULL, ctx.hTimer, INVALID_HANDLE_VALUE);
            ctx.hTimer = NULL;
        }

        CreateTimerQueueTimer(
            &ctx.hTimer,
            NULL,
            [](PVOID lpParam, BOOLEAN) {
                auto* ctx = (ConnectionContext*)lpParam;
                closesocket(ctx->sock);
            },
            (PVOID)&ctx,
            30000, 0, WT_EXECUTEONLYONCE
        );

        string chunk(ioData->data, bytesTransferred);
        if (ctx.headerComplete == false){
            ctx.headerBuffer += chunk;
            size_t pos = ctx.headerBuffer.find("\r\n\r\n");
            if (pos != string::npos) {
                string headers = ctx.headerBuffer.substr(0, pos+4);

                HttpRequest hdrs = HttpRequest(headers);
                ctx.expectedBody = hdrs.contentLength;
                ctx.contentType = hdrs.contentType;
                ctx.headerComplete = true;
                size_t bodyStart = pos+4;
                ctx.bodyBuffer.insert(ctx.bodyBuffer.end(), ctx.headerBuffer.begin()+bodyStart, ctx.headerBuffer.end());
                ctx.headerBuffer = headers;
            }
        }
        else{
            ctx.bodyBuffer.insert(ctx.bodyBuffer.end(), chunk.begin(), chunk.end());
        }
        
        if (ctx.headerComplete && ctx.bodyBuffer.size() >= ctx.expectedBody) {
            HttpRequest req(ctx.headerBuffer, ctx.bodyBuffer);
            HttpResponse res = router.handleRoute(req);
            prepareSendBuffer(ctx, res);
            postSend(clientSocket, ioData, ctx);
        } else {
            // Request not complete yet — post another recv
            postRecv(clientSocket, ioData);
        }
    }
}


void Server::start(int numThreads, int timeout){
    HANDLE hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    if(numThreads==-1){
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numThreads = sysInfo.dwNumberOfProcessors*2;
    }

    vector<thread> workers;
    for (int i = 0; i < numThreads; i++) {
        workers.push_back(thread(&Server::workerThread, this, hiocp));
    }

    log("Serving listening on "+to_string(ntohs(serverAddr.sin_port)));

    std::thread([this] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            ipLimiter.evictStale();
        }
    }).detach();

     while (true) {
        SOCKET clientSock = accept(serverSocket, NULL, NULL);
        //log("Client connected");
        activeClients++;
        //log(to_string(activeClients)+" active clients");
        if (activeClients > MAX_CLIENTS) {
            std::string json = R"({"message":"Server Too Busy. Try again later"})";
            HttpResponse res = JsonResponse(json,429);
            send(clientSock, res.headers().c_str(), res.headers().size(),0);
            send(clientSock, res.body.data(), res.body.size(),0);
            closesocket(clientSock);
            continue;
        }

        sockaddr_in peer;
        int len = sizeof(peer);
        getpeername(clientSock, (sockaddr*)&peer, &len);
        char ipBuf[16];
        InetNtopA(AF_INET, &peer.sin_addr, ipBuf, sizeof(ipBuf));
        CreateIoCompletionPort((HANDLE)clientSock, hiocp, (ULONG_PTR)clientSock, 0);
        auto* connCtx = new ConnectionContext();
        connCtx->windowStart = std::chrono::steady_clock::now();
        connCtx->hTimer = NULL;
        connCtx->sock = clientSock;
        connCtx->peerIp = std::string(ipBuf);
        CreateTimerQueueTimer(
            &connCtx->hTimer,
            NULL,
            [](PVOID lpParam, BOOLEAN) {
                auto* ctx = (ConnectionContext*)lpParam;
                closesocket(ctx->sock);
                //log("Client timed out (30s inactivity)");
            },
            (PVOID)connCtx,
            timeout,
            0,
            WT_EXECUTEONLYONCE
        );
        PER_IO_OPERATION_DATA* ioData = new PER_IO_OPERATION_DATA;
        ZeroMemory(&ioData->overlapped, sizeof(WSAOVERLAPPED));
        ioData->wsabuf.buf = ioData->data;
        ioData->wsabuf.len = sizeof(ioData->data);
        ioData->opType = IoOpType::RECV;
        ioData->connCtx = connCtx;
        DWORD flags = 0;
        int wsares = WSARecv(clientSock, &ioData->wsabuf, 1, NULL, &flags, &ioData->overlapped, NULL);
        if (wsares == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            logerr("Error connecting client");
        }
    }
}