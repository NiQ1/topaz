/**
 *	@file LoginSession.cpp
 *	Login session information and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "LoginSession.h"
#include "new-common/Debugging.h"
#include <mariadb++/connection.hpp>
#include "new-common/Database.h"
#include "LoginGlobalConfig.h"
#include "new-common/Utilities.h"
#include <string.h>

#define LOCK_SESSION std::lock_guard<std::recursive_mutex> l_session(*GetMutex())

LoginSession::LoginSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL) :
    mdwAccountId(dwAccountId),
    mdwIpAddr(dwIpAddr),
    mbKeyInstalled(false),
    mbIgnoreOnIPLookup(false),
    mcNumCharacters(0),
    mcNumCharsAllowed(0),
    mbCharListLoaded(false),
    mdwExpansionsBitmask(0),
    mdwFeaturesBitmask(0),
    mbDataServerFinished(false),
    mbViewServerFinished(false),
    mcMQMessageOriginatingWorld(0)
{
    LOG_DEBUG0("Called.");
    memset(mbufInitialKey, 0, sizeof(mbufInitialKey));
    mtmExpires = time(NULL) + tmTTL;
    memset(mCharacters, 0, sizeof(mCharacters));
}

LoginSession::~LoginSession()
{
    LOG_DEBUG0("Called.");
}

std::recursive_mutex* LoginSession::GetMutex()
{
    LOG_DEBUG0("Called.");
    return &mMutex;
}

uint32_t LoginSession::GetAccountID() const
{
    return mdwAccountId;
}

uint32_t LoginSession::GetClientIPAddress() const
{
    return mdwIpAddr;
}

const uint8_t* LoginSession::GetKey() const
{
    if (mbKeyInstalled == false) {
        LOG_ERROR("Attempted to get the session key before setting it.");
        throw std::runtime_error("Session key not installed.");
    }
    return mbufInitialKey;
}

time_t LoginSession::GetExpiryTime() const
{
    return mtmExpires;
}

bool LoginSession::HasExpired() const
{
    return mtmExpires <= time(NULL);
}

bool LoginSession::GetIgnoreIPLookupFlag() const
{
    return mbIgnoreOnIPLookup;
}

uint8_t LoginSession::GetNumCharacters() const
{
    if (!mbCharListLoaded) {
        LOG_ERROR("Attempted to access character data before loading from DB.");
        throw std::runtime_error("Character data not available");
    }
    return mcNumCharacters;
}

uint8_t LoginSession::GetNumCharsAllowed() const
{
    if (!mbCharListLoaded) {
        LOG_ERROR("Attempted to access character data before loading from DB.");
        throw std::runtime_error("Character data not available");
    }
    return mcNumCharsAllowed;
}

CharMessageHnd::CHARACTER_ENTRY* LoginSession::GetCharacter(uint8_t cOffset)
{
    if (!mbCharListLoaded) {
        LOG_ERROR("Attempted to access character data before loading from DB.");
        throw std::runtime_error("Character data not available");
    }
    return mCharacters + cOffset;
}

std::string LoginSession::GetClientVersion() const
{
    return mstrClientVersion;
}

void LoginSession::SetKey(const uint8_t* bufKey)
{
    memcpy(mbufInitialKey, bufKey, sizeof(mbufInitialKey));
    mbKeyInstalled = true;
}

void LoginSession::SetExpiryTimeAbsolute(time_t tmNewTime)
{
    mtmExpires = tmNewTime;
}

void LoginSession::SetExpiryTimeRelative(time_t tmNewTTL, bool bAllowDecrease)
{
    time_t tmNewExpiry = time(NULL) + tmNewTTL;
    if ((bAllowDecrease) || (tmNewExpiry > mtmExpires)) {
        mtmExpires = tmNewExpiry;
    }
}

void LoginSession::SetIgnoreIPLookupFlag(bool bNewFlag)
{
    mbIgnoreOnIPLookup = bNewFlag;
}

uint32_t LoginSession::GetExpansionsBitmask() const
{
    return mdwExpansionsBitmask;
}

uint32_t LoginSession::GetFeaturesBitmask() const
{
    return mdwFeaturesBitmask;
}

uint32_t LoginSession::GetPrivilegesBitmask() const
{
    return mdwPrivilegesBitmask;
}

void LoginSession::SetExpansionsBitmask(uint32_t dwExpansions)
{
    mdwExpansionsBitmask = dwExpansions;
}

void LoginSession::SetFeaturesBitmask(uint32_t dwFeatures)
{
    mdwFeaturesBitmask = dwFeatures;
}

void LoginSession::SetPrivilegesBitmask(uint32_t dwPrivileges)
{
    mdwFeaturesBitmask = dwPrivileges;
}

void LoginSession::SetClientVersion(std::string& strClientVersion)
{
    mstrClientVersion = strClientVersion;
}

void LoginSession::LoadCharacterList()
{
    LOG_DEBUG0("Called.");
    if (mbCharListLoaded) {
        LOG_DEBUG1("Character list already loaded, will not load again.");
        return;
    }
    LOCK_SESSION;
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    LOCK_DB;
    LOCK_CONFIG;

    // First, query all content ids, which should be in the table even if not
    // yet associated with a character.
    std::string strSqlQueryFmt("SELECT content_id, enabled FROM %sacontents WHERE account_id=%d ORDER BY content_id LIMIT 16;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountId));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    mcNumCharsAllowed = static_cast<uint8_t>(pResultSet->row_count());
    if (mcNumCharsAllowed == 0) {
        LOG_ERROR("No Content IDs associated with the given account");
        throw std::runtime_error("content_id query failed.");
    }
    memset(&mCharacters, 0, sizeof(mCharacters));
    uint32_t i = 0;
    while (pResultSet->next()) {
        if (i >= 16) {
            LOG_WARNING("Too many content IDs associated with the account, ignoring extra content ids!");
            break;
        }
        mCharacters[i].dwContentID = pResultSet->get_unsigned32(0);
        mCharacters[i].bEnabled = pResultSet->get_boolean(1);
        // This tells the client that this content ID is not associated with a character
        // (if it is, it will be overwritten very soon).
        mCharacters[i].szCharName[0] = ' ';
        i++;
    }
    // It's now time to get the actual list of characters
    strSqlQueryFmt = "SELECT content_id, character_id, name, world_id, main_job, main_job_lv, "
        "zone, race, face, hair, head, body, hands, legs, feet, main, sub, size, nation "
        "FROM %schars WHERE content_id IN (SELECT content_id from %scontents WHERE account_id=%d) ORDER BY content_id;";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountId);
    pResultSet = DB->query(strSqlFinalQuery);
    i = 0;
    uint32_t dwCurrentContentID = 0;
    uint32_t j = 0;
    while (pResultSet->next()) {
        dwCurrentContentID = pResultSet->get_unsigned32(0);
        // The character's position in the list must match the content id which is already set-up
        // so we'll need to search for it.
        for (j = 0; j < mcNumCharsAllowed; j++) {
            if (mCharacters[j].dwContentID == dwCurrentContentID) {
                break;
            }
        }
        if (j >= mcNumCharsAllowed) {
            LOG_WARNING("Account has a character without a valid matching content ID, this character will be skipped.");
            continue;
        }
        mCharacters[j].dwCharacterID = pResultSet->get_unsigned32(1);
        strncpy(mCharacters[j].szCharName, pResultSet->get_string(2).c_str(), sizeof(mCharacters[i].szCharName));
        mCharacters[j].cWorldID = static_cast<uint8_t>(pResultSet->get_unsigned32(3));
        mCharacters[j].cMainJob = static_cast<uint8_t>(pResultSet->get_unsigned32(4));
        mCharacters[j].cMainJobLevel = static_cast<uint8_t>(pResultSet->get_unsigned32(5));
        mCharacters[j].wZone = static_cast<uint16_t>(pResultSet->get_unsigned32(6));
        mCharacters[j].cRace = static_cast<uint8_t>(pResultSet->get_unsigned32(7));
        mCharacters[j].cFace = static_cast<uint8_t>(pResultSet->get_unsigned32(8));
        mCharacters[j].cHair = static_cast<uint8_t>(pResultSet->get_unsigned32(9));
        mCharacters[j].wHead = static_cast<uint16_t>(pResultSet->get_unsigned32(10));
        mCharacters[j].wBody = static_cast<uint16_t>(pResultSet->get_unsigned32(11));
        mCharacters[j].wHands = static_cast<uint16_t>(pResultSet->get_unsigned32(12));
        mCharacters[j].wLegs = static_cast<uint16_t>(pResultSet->get_unsigned32(13));
        mCharacters[j].wFeet = static_cast<uint16_t>(pResultSet->get_unsigned32(14));
        mCharacters[j].wMain = static_cast<uint16_t>(pResultSet->get_unsigned32(15));
        mCharacters[j].wSub = static_cast<uint16_t>(pResultSet->get_unsigned32(16));
        mCharacters[j].cSize = static_cast<uint8_t>(pResultSet->get_unsigned32(17));
        mCharacters[j].cNation = static_cast<uint8_t>(pResultSet->get_unsigned32(18));
        i++;
        if (i >= mcNumCharsAllowed) {
            // Safeguard just in case the DB has more chars than allowed
            break;
        }
        mcNumCharacters++;
    }
    mbCharListLoaded = true;
    LOG_DEBUG1("Character list loaded.");
}

void LoginSession::SendRequestToDataServer(REQUESTS_TO_DATA_SERVER State)
{
    mRequestToData = State;
}

void LoginSession::SendRequestToViewServer(REQUESTS_TO_VIEW_SERVER State)
{
    mRequestToView = State;
}

LoginSession::REQUESTS_TO_VIEW_SERVER LoginSession::GetRequestFromDataServer()
{
    return mRequestToView;
}

LoginSession::REQUESTS_TO_DATA_SERVER LoginSession::GetRequestFromViewServer()
{
    return mRequestToData;
}

bool LoginSession::IsDataServerFinished() const
{
    return mbDataServerFinished;
}

bool LoginSession::IsViewServerFinished() const
{
    return mbViewServerFinished;
}

void LoginSession::SetDataServerFinished()
{
    mbDataServerFinished = true;
}

void LoginSession::SetViewServerFinished()
{
    mbViewServerFinished = true;
}

std::shared_ptr<uint8_t> LoginSession::GetMessageFromMQ(uint8_t* pSendingWorld)
{
    std::shared_ptr<uint8_t> pMessage(mpMessageFromMQ);
    if (pSendingWorld) {
        *pSendingWorld = mcMQMessageOriginatingWorld;
    }
    mpMessageFromMQ = NULL;
    return pMessage;
}

void LoginSession::SendMQMessageToViewServer(std::shared_ptr<uint8_t> pMQMessage, uint8_t cSendingWorld)
{
    LOCK_SESSION;
    if (mpMessageFromMQ != NULL) {
        LOG_ERROR("Message sent to session before the previous was read.");
        throw std::runtime_error("Message sent too quickly.");
    }
    mpMessageFromMQ = pMQMessage;
    mcMQMessageOriginatingWorld = cSendingWorld;
}

bool LoginSession::IsCharacterAssociatedWithSession(uint32_t dwCharacterID, uint8_t cWorldID)
{
    uint8_t i = 0;

    LOCK_SESSION;
    for (i = 0; i < mcNumCharsAllowed; i++) {
        if ((mCharacters[i].dwCharacterID == dwCharacterID) && (mCharacters[i].cWorldID == cWorldID)) {
            return true;
        }
    }
    return false;
}

bool LoginSession::IsContentIDAssociatedWithSession(uint32_t dwContentID)
{
    uint8_t i = 0;

    LOCK_SESSION;
    for (i = 0; i < mcNumCharsAllowed; i++) {
        if (mCharacters[i].dwContentID == dwContentID) {
            return true;
        }
    }
    return false;
}

CharMessageHnd::CHARACTER_ENTRY* LoginSession::GetCharacterByContentID(uint32_t dwContentID)
{
    if (!mbCharListLoaded) {
        LOG_ERROR("Attempted to access character data before loading from DB.");
        throw std::runtime_error("Character data not available");
    }
    for (uint8_t i = 0; i < mcNumCharsAllowed; i++) {
        if (mCharacters[i].dwContentID == dwContentID) {
            return &mCharacters[i];
        }
    }
    LOG_ERROR("Content ID did not match any character.");
    throw std::runtime_error("No character matched content ID.");
}
