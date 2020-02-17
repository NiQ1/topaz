/**
 *	@file ProtocolHandler.cpp
 *	Interface to various protocols implemented by the login server.
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "ProtocolHandler.h"
#include "Debugging.h"

ProtocolHandler::ProtocolHandler(std::shared_ptr<TCPConnection> connection) : mpConnection(connection),
mbRunning(false),
mbShutdown(false)
{
    LOG_DEBUG0("Called.");
}

ProtocolHandler::~ProtocolHandler()
{
    LOG_DEBUG0("Called.");
    Shutdown();
}

bool ProtocolHandler::IsRunning() const
{
    return mbRunning;
}

void ProtocolHandler::Shutdown(bool bJoin)
{
    LOG_DEBUG0("Called.");
    if (mbShutdown == false) {
        LOG_DEBUG1("Shutting down handler.");
        mbShutdown = true;
        while (mbRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if ((bJoin) && (mpThreadObj) && (mpThreadObj->joinable())) {
            mpThreadObj->join();
            mpThreadObj = NULL;
            LOG_DEBUG0("Thread joined.");
        }
        mpConnection->Close();
        LOG_DEBUG1("Handler ended successfully.");
    }
}

void ProtocolHandler::StartThread()
{
    LOG_DEBUG0("Called.");
    if (mpThreadObj != NULL) {
        LOG_ERROR("ProtocolHandler thread already running!");
        throw std::runtime_error("Thread already running");
    }
    mpThreadObj = std::shared_ptr<std::thread>(new std::thread(stRun, this));
}

void ProtocolHandler::stRun(ProtocolHandler* thisobj)
{
    thisobj->Run();
}

const BoundSocket& ProtocolHandler::GetClientDetails() const
{
    return mpConnection->GetConnectionDetails();
}
