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

void SessionTracker::InitializeNewSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL)
{
    LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    time_t now = time(NULL);
    auto data = mmapSessions.find(dwAccountId);
    if (data != mmapSessions.end()) {
        LOG_INFO("Session already exists");
        // A session has already been found for the said account
        if (data->second->GetClientIPAddress() == dwIpAddr) {
            // Matches the IP address we have so just increase the TTL if needed
            // and return silently.
            data->second->SetExpiryTimeRelative(tmTTL);
        }
        // If it's from a different IP then this is an error
        LOG_ERROR("Received a session request for the same account from different IP address.");
        throw std::runtime_error("Session exists with different IP");
    }
    LOG_INFO("Creating new session.");
    LoginSession* NewSession = new LoginSession(dwAccountId, dwIpAddr, tmTTL);
    mmapSessions[dwAccountId] = std::shared_ptr<LoginSession>(NewSession);
}

std::shared_ptr<LoginSession> SessionTracker::GetSessionDetails(uint32_t dwAccountId)
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

std::shared_ptr<LoginSession> SessionTracker::LookupSessionByIP(uint32_t dwIPAddress)
{
    LOG_DEBUG0("Called.");
    auto i = mmapSessions.begin();
    while (i != mmapSessions.end()) {
        if ((i->second->GetClientIPAddress() == dwIPAddress) && (i->second->GetIgnoreIPLookupFlag() == false)) {
            return i->second;
        }
        i++;
    }
    LOG_WARNING("Session not found for given IP address.");
    throw std::runtime_error("Session ID not found");
}

void SessionTracker::SetSessionDetails(std::shared_ptr<LoginSession> SessionData)
{
    LOG_DEBUG0("Called.");
    LOCK_TRACKER;
    mmapSessions[SessionData->GetAccountID()] = SessionData;
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
        if (i->second->HasExpired()) {
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
