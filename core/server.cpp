#include "server.hpp"
#include<iostream>
#include <vector>
#include <thread>
#include<mutex>
#include <ws2tcpip.h>
#include "logging.hpp"
using namespace std;

void Server::workerThread(HANDLE hiocp){
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    PER_IO_OPERATION_DATA* ioData;

    while(true){
        BOOL ok = GetQueuedCompletionStatus(hiocp, &bytesTransferred, &completionKey, (LPOVERLAPPED*)&ioData, INFINITE);
        SOCKET clientSocket = (SOCKET)completionKey;
        if(!ok || bytesTransferred==0){
            //log("Client Disconnected");
            activeClients--;
            closesocket(clientSocket);
            if (ioData->connCtx->hTimer) {
                DeleteTimerQueueTimer(NULL, ioData->connCtx->hTimer, INVALID_HANDLE_VALUE);
                ioData->connCtx->hTimer = NULL;
            }
            delete ioData->connCtx;
            delete ioData;
            continue;
        }

        auto& ctx = *ioData->connCtx;
        auto now = std::chrono::steady_clock::now();

        bool allowed = true;
        allowed = ipLimiter.allowRequest(ctx.peerIp);
        if (!allowed) {
            logerr("Adaptive rate limit exceeded, sending 429");
            std::string json = R"({"message":"Rate Limit Exceeded. Try again later."})";
            HttpResponse res = JsonResponse(json, 429);
            send(clientSocket, res.headers().c_str(), res.headers().size(), 0);
            send(clientSocket, res.body.data(), res.body.size(), 0);
            continue;
        }

        if (ioData->connCtx->hTimer) {
            DeleteTimerQueueTimer(NULL, ioData->connCtx->hTimer, INVALID_HANDLE_VALUE);
            ioData->connCtx->hTimer = NULL;
        }

        CreateTimerQueueTimer(
            &ctx.hTimer,
            NULL,
            [](PVOID lpParam, BOOLEAN) {
                auto* ctx = (ConnectionContext*)lpParam;
                closesocket(ctx->sock);
                //log("Client timed out (30s inactivity)");
            },
            (PVOID)&ctx,
            30000, 0, WT_EXECUTEONLYONCE
        );

        string  chunk(ioData->data, bytesTransferred);
        if (ctx.headerComplete==false){
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
            //log("Request recieved:");
            //log(ctx.headerBuffer);
            //log("--------------");
            HttpRequest req(ctx.headerBuffer, ctx.bodyBuffer);
            HttpResponse res = router.handleRoute(req);
            send(clientSocket, res.headers().c_str(), res.headers().size(),0);
            send(clientSocket, res.body.data(), res.body.size(),0);
            ctx.headerBuffer.clear();
            ctx.bodyBuffer.clear();
            ctx.expectedBody = 0;
            ctx.headerComplete = false;
        }
        ZeroMemory(&ioData->overlapped, sizeof(WSAOVERLAPPED));
        ioData->buffer.buf = ioData->data;
        ioData->buffer.len = sizeof(ioData->data);
        DWORD flags = 0;
        int wsares = WSARecv(clientSocket, &ioData->buffer, 1, NULL, &flags, &ioData->overlapped, NULL);
        if (wsares == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            logerr("Error connecting client");
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
        ioData->buffer.buf = ioData->data;
        ioData->buffer.len = sizeof(ioData->data);
        ioData->connCtx = connCtx;
        DWORD flags = 0;
        int wsares = WSARecv(clientSock, &ioData->buffer, 1, NULL, &flags, &ioData->overlapped, NULL);
        if (wsares == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            logerr("Error connecting client");
        }
    }
}