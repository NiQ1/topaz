/**
 *	@file LoginHandler.cpp
 *	Implementation of the login server protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "LoginHandler.h"
#include "Debugging.h"
#include <thread>
#include <chrono>

LoginHandler::LoginHandler(TCPConnection& connection) : mConnection(connection),
    mbRunning(false),
    mbShutdown(false)
{
    LOG_DEBUG0("Called.");
}

LoginHandler::~LoginHandler()
{
    LOG_DEBUG0("Called.");
    Shutdown();
}

void LoginHandler::Run()
{
    LOG_DEBUG0("Called.");
    mbRunning = true;
    LOG_INFO("Handling connection.");
    // Do stuff here
    mbRunning = false;
}

bool LoginHandler::IsRunning() const
{
    return mbRunning;
}

void LoginHandler::Shutdown(bool bJoin)
{
    LOG_DEBUG0("Called.");
    if (mbShutdown == false) {
        LOG_DEBUG1("Shutting down handler.");
        mbShutdown = true;
        while (mbRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if ((bJoin) && (mpThreadObj) && (mpThreadObj->joinable())) {
                mpThreadObj->join();
            }
        }
        mConnection.Close();
        LOG_DEBUG1("Handler ended successfully.");
    }
}

void LoginHandler::stRun(LoginHandler* thisobj)
{
    thisobj->Run();
}

void LoginHandler::AttachThreadObject(std::thread* pThreadObj)
{
    mpThreadObj = std::shared_ptr<std::thread>(pThreadObj);
}
