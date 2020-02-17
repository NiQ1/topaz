/**
 *	@file SessionTracker.cpp
 *	Keeps track of open sessions, allows cross referencing between protocols
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "SessionTracker.h"
#include "Debugging.h"
#include <stdexcept>
#include <time.h>

SessionTrackerPtr SessionTracker::smpSingletonObj = NULL;
bool SessionTracker::sbBeingDestroyed = false;

SessionTrackerPtr SessionTracker::GetInstance()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new SessionTracker;
    }
    return smpSingletonObj;
}

std::mutex* SessionTracker::GetMutex()
{
    // Commented out - Spams the log
    // LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access session tracker before initialzing.");
        throw std::runtime_error("Tracker not initialized.");
    }
    return &smpSingletonObj->mMutex;
}

void SessionTracker::Destroy()
{
    LOG_DEBUG0("Called.");
    sbBeingDestroyed = true;
    if (smpSingletonObj != NULL) {
        LOG_DEBUG1("Deleting session tracker.");
        delete smpSingletonObj;
    }
}

SessionTracker::SessionTracker()
{
    LOG_DEBUG0("Called.");
}

SessionTracker::~SessionTracker()
{
    if (sbBeingDestroyed == false) {
        Destroy();
    }
}

SessionTracker::SESSION_DATA SessionTracker::GetSessionDetails(uint32_t dwAccountId)
{
    LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    auto data = mmapSessions.find(dwAccountId);
    if (data != mmapSessions.end()) {
        LOG_DEBUG1("Session found for account ID: %d", dwAccountId);
        return data->second;
    }
    LOG_WARNING("Session ID not found for accout: %d", dwAccountId);
    throw std::runtime_error("Session ID not found");
}

void SessionTracker::SetSessionDetails(SessionTracker::SESSION_DATA SessionData)
{
    LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    mmapSessions[SessionData.dwIpAddr] = SessionData;
}

void SessionTracker::DeleteSession(uint32_t dwAccountId)
{
    LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    if (mmapSessions.erase(dwAccountId) == 0) {
        LOG_ERROR("Attempted to delete a nonexistent session.");
        throw std::runtime_error("Session ID not found");
    }
}

void SessionTracker::DeleteExpiredSessions()
{
    // Commented out - Spams the log
    // LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    time_t tmNow = time(NULL);

    auto i = mmapSessions.begin();
    auto j = i;
    while (i != mmapSessions.end()) {
        if (i->second.tmExpires <= tmNow) {
            j = i;
            j++;
            mmapSessions.erase(i->first);
            i = j;
        }
        else {
            i++;
        }
    }
}
