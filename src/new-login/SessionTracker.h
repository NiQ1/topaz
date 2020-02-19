/**
 *	@file SessionTracker.h
 *	Keeps track of open sessions, allows cross referencing between protocols
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_SESSIONTRACKER_H
#define FFXI_LOGIN_SESSIONTRACKER_H

#include "LoginSession.h"
#include <stddef.h>
#include <mutex>
#include <memory>
#include <map>

class SessionTracker;
typedef SessionTracker* SessionTrackerPtr;

// Easy way to lock the session mutex
#define LOCK_TRACKER std::lock_guard<std::mutex> l_tracker(*SessionTracker::GetMutex())

/**
 *  Session tracker singleton class
 */
class SessionTracker
{
public:

    /**
     *  Return an instance to the singleton.
     *  @return Singleton instance of this class.
     */
    static SessionTrackerPtr GetInstance();

    /**
     *  Gets the global tracker Mutex object. Automatically locjked
     *  upon access so generally does not need to be manually called.
     *  @return Database mutex object.
     */
    static std::mutex* GetMutex();

    /**
     *  Destroy the singleton,
     *  should only be called when the server is shutting down.
     */
    void Destroy();

    /**
     *  Destructor. It is preferrable to call destroy explicitly.
     */
    ~SessionTracker();

    /**
     *  Initialize a new session given the required initial values.
     *  @param dwAccountId Account ID of the session being created
     *  @param dwIpAddr IP address of the client
     *  @param tmTTL TTL for the session before it's autodeleted (default = 30 seconds)
     *  @return The newly created session
     */
    std::shared_ptr<LoginSession> InitializeNewSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL = 30);

    /**
     *  Get a session data struct containing the details of a given session.
     *  @param dwAccountId Account ID to look up
     *  @return Session details struct
     */
    std::shared_ptr<LoginSession> GetSessionDetails(uint32_t dwAccountId);

    /**
     *  Look-up session details by a given IP address.
     *  @param dwIPAddress IP address (network byte order)
     *  @return Session details struct
     */
    std::shared_ptr<LoginSession> LookupSessionByIP(uint32_t dwIPAddress);

    /**
     *  Add or change session details.
     *  @param SessionData New/updated session data to store
     */
    void SetSessionDetails(std::shared_ptr<LoginSession> SessionData);

    /**
     *  Manually delete a session.
     *  @param dwAccountId Account ID whose session is to be deleted.
     */
    void DeleteSession(uint32_t dwAccountId);

    /**
     *  Delete all expired sessions.
     */
    void DeleteExpiredSessions();

private:
    /**
     *  Private constructor, used by GetInstance to initiate
     *  the tracker when called for the first time.
     */
    SessionTracker();

    /// Static singleton pointer
    static SessionTrackerPtr smpSingletonObj;
    /// Object is being destroyed
    static bool sbBeingDestroyed;

    /// All sessions currently being tracked
    std::map<uint32_t, std::shared_ptr<LoginSession>> mmapSessions;
    /// Tracker access mutex
    std::mutex mMutex;
};

#endif
