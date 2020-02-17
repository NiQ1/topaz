/**
 *	@file SessionTracker.h
 *	Keeps track of open sessions, allows cross referencing between protocols
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_SESSIONTRACKER_H
#define FFXI_LOGIN_SESSIONTRACKER_H

#include <stddef.h>
#include <mutex>
#include <memory>
#include <map>

class SessionTracker;
typedef SessionTracker* SessionTrackerPtr;

// Easy way to lock the session mutex
#define LOCK_TRACKER std::lock_guard<std::mutex> l(*SessionTracker::GetMutex())

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
     *  Session data
     */
    struct SESSION_DATA
    {
        // Account ID received from authentication
        uint32_t dwAccountId;
        // IP address the user is connecting from
        uint32_t dwIpAddr;
        // Initial key to be sent to the map server
        uint8_t bufInitialKey[24];
        // When the session expires and auto-removed
        time_t tmExpires;
    };

    /**
     *  Get a session data struct containing the details of a given session.
     *  @param dwAccountId Account ID to look up
     *  @return Session details struct
     */
    SESSION_DATA GetSessionDetails(uint32_t dwAccountId);

    /**
     *  Add or change session details.
     *  @param SessionData New/updated session data to store
     */
    void SetSessionDetails(SESSION_DATA SessionData);

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
    std::map<uint32_t, SESSION_DATA> mmapSessions;
    /// Database access mutex
    std::mutex mMutex;
};

#endif
