/**
 *	@file LoginServer.cpp
 *	Easy threading interface
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "Thread.h"
#include "Debugging.h"
#include <stdexcept>

Thread::Thread() : mbRunning(false), mbShutdown(false)
{
    LOG_DEBUG0("Called.");
}

Thread::~Thread()
{
    LOG_DEBUG0("Called.");
    Shutdown();
}

bool Thread::IsRunning() const
{
    return mbRunning;
}

void Thread::Shutdown(bool bJoin)
{
    LOG_DEBUG0("Called.");
    if (mbShutdown == false) {
        LOG_DEBUG1("Stopping thread.");
        mbShutdown = true;
        while (mbRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if ((bJoin) && (mpThreadObj) && (mpThreadObj->joinable())) {
            mpThreadObj->join();
            mpThreadObj = NULL;
            LOG_DEBUG0("Thread joined.");
        }
        LOG_DEBUG1("Thread ended successfully.");
    }
}

void Thread::StartThread()
{
    LOG_DEBUG0("Called.");
    if (mpThreadObj != NULL) {
        LOG_ERROR("Thread already running!");
        throw std::runtime_error("Thread already running");
    }
    mpThreadObj = std::shared_ptr<std::thread>(new std::thread(stRun, this));
}

void Thread::stRun(Thread* thisobj)
{
    try {
        thisobj->Run();
    }
    catch (...) {
        LOG_ERROR("Uncaught exception in thread!");
    }
}
