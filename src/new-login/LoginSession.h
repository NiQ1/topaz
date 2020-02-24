/**
 *	@file LoginSession.h
 *	Login session information and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_LOGINSESSION_H
#define FFXI_LOGIN_LOGINSESSION_H

#include <stdint.h>
#include <time.h>
#include <mutex>
#include <queue>

/**
 *  Represents a single open session.
 */
class LoginSession
{
public:
    /**
     *  Initialize a new session given the required initial values.
     *  @param dwAccountId Account ID of the session being created
     *  @param dwIpAddr IP address of the client
     *  @param tmTTL TTL for the session before it's autodeleted (default = 30 seconds)
     */
    LoginSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL = 30);

    /**
     *  Destructor
     */
    ~LoginSession();

    /**
     *  Gets the session mutex object. Lock this before doing any changes.
     *  @return Session mutex object.
     */
    std::recursive_mutex* GetMutex();

    /**
     *  Get the account ID associated with the session
     *  @return Account ID
     */
    uint32_t GetAccountID() const;

    /**
     *  Get the IP address associated with the session
     *  @return IP address in network byte order
     */
    uint32_t GetClientIPAddress() const;

    /**
     *  Get the encryption key associated with the session
     *  @return Pointer to the key buffer
     */
    const uint8_t* GetKey() const;

    /**
     *  Get the expiry time of the session
     *  @return Expity time as UNIX timestamp
     */
    time_t GetExpiryTime() const;

    /**
     *  Check whether the session has expired
     *  @return True if expired, false otherwise
     */
    bool HasExpired() const;

    /**
     *  Checks whether this session should not be included in IP lookups.
     *  @return True if the session is to be ignored, false otherwise
     */
    bool GetIgnoreIPLookupFlag() const;

    /**
     *  Gets the number of characters the user already has.
     *  @return Number of characters
     */
    uint8_t GetNumCharacters() const;

    /**
     *  Returns the maximum number of characters this account can have.
     *  @return Number of accounts allowed.
     */
    uint8_t GetNumCharsAllowed() const;

    /**
     *  Get the allowed expansions for this user
     *  @return Expansions bitmask
     */
    uint32_t GetExpansionsBitmask() const;

    /**
     *  Get the account features on this account
     *  @return Features bitmask
     */
    uint32_t GetFeaturesBitmask() const;

    /**
     *  Get the account privileges on this account
     *  @return Privileges bitmask
     */
    uint32_t GetPrivilegesBitmask() const;

    /**
     *  Set the encryption key for this session.
     *  @param bufKey 24 byte long key buffer
     */
    void SetKey(const uint8_t* bufKey);

    /**
     *  Manually set the expiry time to the given value
     *  @param tmNewTime New expiry time to set
     */
    void SetExpiryTimeAbsolute(time_t tmNewTime);

    /**
     *  Set the expiry time relative to the current time
     *  @param tmNewTTL New TTL of the session in seconds
     *  @param bAllowDecrease Whether TTL can be decreased from its current value
     */
    void SetExpiryTimeRelative(time_t tmNewTTL, bool bAllowDecrease = false);

    /**
     *  Sets whether this session should not be included in IP lookups.
     *  @param bNewFlag Set to true to exclude the session from IP lookups
     */
    void SetIgnoreIPLookupFlag(bool bNewFlag);

    /**
     *  Set the allowed expansions for this user
     *  @param dwExpansions New expansions bitmask
     */
    void SetExpansionsBitmask(uint32_t dwExpansions);

    /**
     *  Get the account features on this account
     *  @param dwFeatures New features bitmask
     */
    void SetFeaturesBitmask(uint32_t dwFeatures);

    /**
     *  Get the account privileges on this account
     *  @param dwFeatures New privileges bitmask
     */
    void SetPrivilegesBitmask(uint32_t dwPrivileges);

    /**
     *  A single entry in the account character list
     */
    struct CHARACTER_ENTRY
    {
        // Always zero, doesn't seem to have any meaning
        uint32_t dwCharacterID;
        char szCharName[16];
        uint32_t dwAccountID;
        uint8_t cWorldID;
        uint8_t cMainJob;
        uint8_t cMainJobLevel;
        uint16_t wZone;
        uint8_t cRace;
        uint16_t wFace;
        // Whatever the char was wearing when last logged-out
        uint16_t wHead;
        uint16_t wBody;
        uint16_t wHands;
        uint16_t wLegs;
        uint16_t wFeet;
        // Equipped weapons, not jobs
        uint16_t wMain;
        uint16_t wSub;
    };

    /**
     *  Load the character list from the DB
     */
    void LoadCharacterList();

    /**
     *  Get a character entry struct
     *  @param cOffset Offset in the character list
     *  @return Character entry struct with character data
     */
    const CHARACTER_ENTRY* GetCharacter(uint8_t cOffset);

    /**
     *  List of requests that go between the data and view servers.
     */
    enum DATA_VIEW_REQUESTS {
        NO_REQUEST = 0,
        VIEW_SEND_CHARACTER_LIST
    };

    /**
     *  Issued by the view server to send a request to the data server
     *  @param Request request type to send
     */
    void SendRequestToDataServer(DATA_VIEW_REQUESTS Request);

    /**
     *  Issued by the data server to send a request to the view server
     *  @param Request request type to send
     */
    void SendRequestToViewServer(DATA_VIEW_REQUESTS Request);

    /**
     *  Issued by the view server to check and receive messages from
     *  the data server.
     *  @return Pending request or NO_REQUEST if none
     */
    DATA_VIEW_REQUESTS GetRequestFromDataServer();

    /**
     *  Issued by the data server to check and receive messages from
     *  the view server.
     *  @return Pending request or NO_REQUEST if none
     */
    DATA_VIEW_REQUESTS GetRequestFromViewServer();

private:
    // Account ID received from authentication
    uint32_t mdwAccountId;
    // IP address the user is connecting from
    uint32_t mdwIpAddr;
    // Initial key to be sent to the map server
    uint8_t mbufInitialKey[24];
    // When the session expires and auto-removed
    time_t mtmExpires;
    // Whether to ignore this packet when doing IP lookups
    bool mbIgnoreOnIPLookup;
    // Number of characters the user has
    uint8_t mcNumCharacters;
    // Number of characters the user is allowed
    uint8_t mcNumCharsAllowed;
    // Account expansion bitmask
    uint32_t mdwExpansionsBitmask;
    // Account features bitmask
    uint32_t mdwFeaturesBitmask;
    // Account privileges bitmask
    uint32_t mdwPrivilegesBitmask;
    // Character data list
    CHARACTER_ENTRY mCharacters[16];
    // Whether character list has been loaded
    bool mbCharListLoaded = false;
    // Mutex for access sync
    std::recursive_mutex mMutex;
    // Requests to the data server
    std::queue<DATA_VIEW_REQUESTS> mRequestsToData;
    // Requests to the view server
    std::queue<DATA_VIEW_REQUESTS> mRequestsToView;
};

#endif
