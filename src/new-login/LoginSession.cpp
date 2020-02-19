/**
 *	@file LoginSession.cpp
 *	Login session information and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "LoginSession.h"
#include "Debugging.h"
#include <mariadb++/connection.hpp>
#include "Database.h"
#include "GlobalConfig.h"
#include "Utilities.h"
#include <string.h>

#define LOCK_SESSION std::lock_guard<std::recursive_mutex> l_session(*GetMutex())

LoginSession::LoginSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL) :
    mdwAccountId(dwAccountId),
    mdwIpAddr(dwIpAddr),
    mbIgnoreOnIPLookup(false),
    mcNumCharacters(0),
    mcNumCharsAllowed(0),
    mbCharListLoaded(false),
    mdwExpansionsBitmask(0),
    mdwFeaturesBitmask(0)
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

const LoginSession::CHARACTER_ENTRY* LoginSession::GetCharacter(uint8_t cOffset)
{
    if (!mbCharListLoaded) {
        LOG_ERROR("Attempted to access character data before loading from DB.");
        throw std::runtime_error("Character data not available");
    }
    return mCharacters + cOffset;
}

void LoginSession::SetKey(const uint8_t* bufKey)
{
    memcpy(mbufInitialKey, bufKey, sizeof(mbufInitialKey));
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

void LoginSession::LoadCharacterList()
{
    LOG_DEBUG0("Called.");
    if (mbCharListLoaded) {
        LOG_DEBUG1("Character list already loaded, will not load again.");
        return;
    }
    LOCK_SESSION;
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();
    LOCK_DB;
    LOCK_CONFIG;

    // The size of the packet is determined by the number of characters
    // the user is allowed to create, which is the content_ids column
    // in the accounts table.
    std::string strSqlQueryFmt("SELECT content_ids FROM %saccounts WHERE id=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountId));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Failed to query the number of content IDs.");
        throw std::runtime_error("content_ids query failed.");
    }
    pResultSet->next();
    mcNumCharsAllowed = static_cast<uint8_t>(pResultSet->get_unsigned32(0));
    CHARACTER_ENTRY CharList[16];
    memset(&CharList, 0, sizeof(CharList));
    strSqlQueryFmt = "SELECT id, name, account_id, world_id, main_job, main_job_lv, "
        "zone, race, face, head, body, hands, legs, feet, main, sub "
        "FROM %schars WHERE account_id=%d LIMIT %u;";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountId, static_cast<uint32_t>(mcNumCharsAllowed));
    pResultSet = DB->query(strSqlFinalQuery);
    uint8_t cNumchars = 0;
    uint32_t dwCurrentChar = 0;
    while (pResultSet->next()) {
        dwCurrentChar = pResultSet->get_unsigned32(0);
        mCharacters[cNumchars].dwCharacterID = dwCurrentChar;
        CharList[cNumchars].dwCharacterID = dwCurrentChar;
        strncpy(CharList[cNumchars].szCharName, pResultSet->get_string(1).c_str(), sizeof(CharList[cNumchars].szCharName));
        CharList[cNumchars].dwAccountID = pResultSet->get_unsigned32(2);
        CharList[cNumchars].cWorldID = static_cast<uint8_t>(pResultSet->get_unsigned32(3));
        CharList[cNumchars].cMainJob = static_cast<uint8_t>(pResultSet->get_unsigned32(4));
        CharList[cNumchars].cMainJobLevel = static_cast<uint8_t>(pResultSet->get_unsigned32(5));
        CharList[cNumchars].wZone = static_cast<uint16_t>(pResultSet->get_unsigned32(6));
        CharList[cNumchars].cRace = static_cast<uint8_t>(pResultSet->get_unsigned32(7));
        CharList[cNumchars].wFace = static_cast<uint16_t>(pResultSet->get_unsigned32(8));
        CharList[cNumchars].wHead = static_cast<uint16_t>(pResultSet->get_unsigned32(9));
        CharList[cNumchars].wBody = static_cast<uint16_t>(pResultSet->get_unsigned32(10));
        CharList[cNumchars].wHands = static_cast<uint16_t>(pResultSet->get_unsigned32(11));
        CharList[cNumchars].wLegs = static_cast<uint16_t>(pResultSet->get_unsigned32(12));
        CharList[cNumchars].wFeet = static_cast<uint16_t>(pResultSet->get_unsigned32(13));
        CharList[cNumchars].wMain = static_cast<uint16_t>(pResultSet->get_unsigned32(14));
        CharList[cNumchars].wSub = static_cast<uint16_t>(pResultSet->get_unsigned32(15));
        cNumchars++;
        if (cNumchars >= mcNumCharsAllowed) {
            // Safeguard just in case the DB has more chars than allowed
            break;
        }
    }
    mbCharListLoaded = true;
    LOG_DEBUG1("Character list loaded.");
}

void LoginSession::SendRequestToDataServer(LoginSession::DATA_VIEW_REQUESTS Request)
{
    LOCK_SESSION;
    mRequestsToData.push(Request);
}

void LoginSession::SendRequestToViewServer(LoginSession::DATA_VIEW_REQUESTS Request)
{
    LOCK_SESSION;
    mRequestsToView.push(Request);
}

LoginSession::DATA_VIEW_REQUESTS LoginSession::GetRequestFromDataServer()
{
    LOCK_SESSION;
    DATA_VIEW_REQUESTS Request = mRequestsToView.front();
    mRequestsToView.pop();
    return Request;
}

LoginSession::DATA_VIEW_REQUESTS LoginSession::GetRequestFromViewServer()
{
    LOCK_SESSION;
    DATA_VIEW_REQUESTS Request = mRequestsToData.front();
    mRequestsToData.pop();
    return Request;
}
