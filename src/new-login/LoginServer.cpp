/**
 *	@file LoginServer.cpp
 *	TCP server routines
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "LoginServer.h"
#include "Debugging.h"

#include <stdexcept>
#include <thread>
#include <chrono>

#include <sys/types.h>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

// Number of allowed pending connections
#define LISTEN_ALLOWED_BACKLOG 10

LoginServer::LoginServer() : mbShutdown(false), mbRunning(false)
{
	LOG_DEBUG0("Called.");
#ifdef _WIN32
	WSADATA WsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0) {
		LOG_CRITICAL("WSAStartup failed.");
		throw std::runtime_error("WSAStartup failed.");
	}
#endif
}

LoginServer::~LoginServer()
{
	LOG_DEBUG0("Called.");
	Shutdown();
#ifdef _WIN32
	WSACleanup();
#endif
}

void LoginServer::AddBind(uint16_t wPortNum, const char* szIpAddress, bool bSecure)
{
	struct BoundSocket NewBind = { 0 };

	LOG_DEBUG0("Called.");
	if (wPortNum == 0) {
		LOG_ERROR("Called with port set to zero.");
		throw std::invalid_argument("Port number cannot be zero.");
	}
	NewBind.BindDetails.sin_family = AF_INET;
	if (szIpAddress) {
		NewBind.BindDetails.sin_addr.s_addr = inet_addr(szIpAddress);
	}
	NewBind.BindDetails.sin_port = htons(wPortNum);
	NewBind.bSecure = bSecure;

	NewBind.iSock = socket(AF_INET, SOCK_STREAM, 0);
	if (NewBind.iSock == -1) {
		LOG_ERROR("socket function failed.");
		throw std::runtime_error("socket function failed.");
	}
	if (bind(NewBind.iSock, (sockaddr*)&NewBind.BindDetails, sizeof(NewBind.BindDetails)) != 0) {
		LOG_ERROR("bid function failed.");
		throw std::runtime_error("bind function failed.");
	}
	if (listen(NewBind.iSock, LISTEN_ALLOWED_BACKLOG) != 0) {
		LOG_ERROR("listen function failed.");
		throw std::runtime_error("listen function failed.");
	}
	LOG_INFO("Attached to %s:%d", inet_ntoa(NewBind.BindDetails.sin_addr), NewBind.BindDetails.sin_port);
	mvecListeningSockets.push_back(NewBind);
}

void LoginServer::Run()
{
	// Socket descriptors used for select call
	fd_set SocketDescriptors;
	// Max socket descriptor used for select call
	int nfds = 0;
	// Iterator
	size_t i = 0;
	// Number of listening sockets
	size_t iNumListeningSocks = 0;
	// Current socket being iterated
	SOCKET sockCurrentSocket = 0;
	// Timeout for select calls
	struct timeval tv = { 0, 1000 };
	// Size of saddrNewConnection
	int cbsaddrNewConnection = 0;
	// New bound socket for incoming connections
	BoundSocket NewConnection = { 0 };

	LOG_DEBUG0("Called.");
	if (mvecListeningSockets.empty()) {
		LOG_CRITICAL("Called without any listening socket.");
		throw std::logic_error("Cannot run server without listening sockets.");
	}
	mbRunning = true;
	LOG_INFO("Server running.");
	while (mbShutdown == false) {
		// Check if a new connnection has arrived
		iNumListeningSocks = mvecListeningSockets.size();
		nfds = 0;
		FD_ZERO(&SocketDescriptors);
		for (i = 0; i < iNumListeningSocks; i++) {
			sockCurrentSocket = mvecListeningSockets[i].iSock;
			FD_SET(sockCurrentSocket, &SocketDescriptors);
			if (sockCurrentSocket > nfds) {
				nfds = static_cast<int>(sockCurrentSocket);
			}
		}
		if (select(nfds, &SocketDescriptors, NULL, NULL, &tv) < 0) {
			LOG_CRITICAL("select function failed.");
			throw std::runtime_error("select function failed.");
		}
		for (i = 0; i < iNumListeningSocks; i++) {
			sockCurrentSocket = mvecListeningSockets[i].iSock;
			if (FD_ISSET(sockCurrentSocket, &SocketDescriptors)) {
				// New connection has arrived
				memset(&NewConnection, 0, sizeof(NewConnection));
				cbsaddrNewConnection = static_cast<int>(sizeof(NewConnection.BindDetails));
				NewConnection.iSock = accept(sockCurrentSocket, reinterpret_cast<struct sockaddr*>(&NewConnection.BindDetails), &cbsaddrNewConnection);
				if (NewConnection.iSock < 0) {
					// Call failed but it's not determinal to the overall server
					LOG_ERROR("Attempted to accept new connection but accept function failed.");
					continue;
				}
				LOG_INFO("Accepted connection from %s", inet_ntoa(NewConnection.BindDetails.sin_addr));
				// TODO: Implement SSL here
				NewConnection.bSecure = false;
				TCPConnection NewTCPConnection(NewConnection);
                // Launch login handler for this connection
                LoginHandler* pNewHandler = new LoginHandler(NewTCPConnection);
                pNewHandler->StartThread();
                mvecWorkingHandlers.push_back(std::shared_ptr<LoginHandler>(pNewHandler));
			}
		}
        // Clean up already finished threads from the vector
        i = 0;
        while (i < mvecWorkingHandlers.size()) {
            if (mvecWorkingHandlers[i]->IsRunning() == false) {
                mvecWorkingHandlers.erase(mvecWorkingHandlers.begin()+i);
            }
            else {
                i++;
            }
        }
	}
	mbRunning = false;
}

bool LoginServer::IsRunning() const
{
    return mbRunning;
}

void LoginServer::StartThread()
{
    LOG_DEBUG0("Called.");
    if (mpThreadObj != NULL) {
        LOG_ERROR("LoginServer thread already running!");
        throw std::runtime_error("Thread already running");
    }
    mpThreadObj = std::shared_ptr<std::thread>(new std::thread(stRun, this));
}

void LoginServer::Shutdown(bool bJoin)
{
	LOG_DEBUG0("Called.");
	if (mbShutdown == false) {
		mbShutdown = true;
		// Wait for server process to end
		while (mbRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		LOG_INFO("All running threads stopped.");
		while (mvecWorkingHandlers.empty() == false) {
			// Since the server is no longer running we can assume these
			// non-atomic operations are safe.
			mvecWorkingHandlers.back()->Shutdown();
			mvecWorkingHandlers.pop_back();
		}
        if ((bJoin) && (mpThreadObj) && (mpThreadObj->joinable())) {
            mpThreadObj->join();
            mpThreadObj = NULL;
            LOG_DEBUG0("Thread joined.");
        }
    }
	LOG_INFO("Server successfully shut down.");
}

void LoginServer::stRun(LoginServer* thisobj)
{
    thisobj->Run();
}
