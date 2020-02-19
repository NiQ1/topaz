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

LoginSession::LoginSession(uint32_t dwAccountId, uint32_t dwIpAddr, time_t tmTTL) :
    mdwAccountId(dwAccountId),
    mdwIpAddr(dwIpAddr),
    mbIgnoreOnIPLookup(false),
    mcNumCharacters(0),
    mcNumCharsAllowed(0),
    mbCharListLoaded(false)
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

void LoginSession::LoadCharacterList()
{
    LOG_DEBUG0("Called.");
    if (mbCharListLoaded) {
        LOG_WARNING("Character list already loaded, will not load again.");
        return;
    }
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();

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
    mcNumCharsAllowed = pResultSet->get_unsigned8(0);
    CHARACTER_ENTRY CharList[16];
    memset(&CharList, 0, sizeof(CharList));
    strSqlQueryFmt = "SELECT id, name, account_id, world_id, main_job, main_job_lv, "
        "zone, race, face, head, body, heands, legs, feet, main, sub "
        "FROM %schars WHERE account_id=%d LIMIT %u;";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountId, static_cast<uint32_t>(mcNumCharsAllowed));
    pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Failed to query the list of characters.");
        throw std::runtime_error("char ids query failed.");
    }
    uint8_t cNumchars = 0;
    uint32_t dwCurrentChar = 0;
    while (pResultSet->next()) {
        dwCurrentChar = pResultSet->get_unsigned32(0);
        mCharacters[cNumchars].dwCharacterID = dwCurrentChar;
        CharList[cNumchars].dwCharacterID = dwCurrentChar;
        strncpy(CharList[cNumchars].szCharName, pResultSet->get_string(1).c_str(), sizeof(CharList[cNumchars].szCharName));
        CharList[cNumchars].dwAccountID = pResultSet->get_unsigned32(2);
        CharList[cNumchars].cWorldID = pResultSet->get_unsigned8(3);
        CharList[cNumchars].cMainJob = pResultSet->get_unsigned8(4);
        CharList[cNumchars].cMainJobLevel = pResultSet->get_unsigned8(5);
        CharList[cNumchars].wZone = pResultSet->get_unsigned16(6);
        CharList[cNumchars].cRace = pResultSet->get_unsigned8(7);
        CharList[cNumchars].wFace = pResultSet->get_unsigned16(8);
        CharList[cNumchars].wHead = pResultSet->get_unsigned16(9);
        CharList[cNumchars].wBody = pResultSet->get_unsigned16(10);
        CharList[cNumchars].wHands = pResultSet->get_unsigned16(11);
        CharList[cNumchars].wLegs = pResultSet->get_unsigned16(12);
        CharList[cNumchars].wFeet = pResultSet->get_unsigned16(13);
        CharList[cNumchars].wMain = pResultSet->get_unsigned16(14);
        CharList[cNumchars].wSub = pResultSet->get_unsigned16(15);
        cNumchars++;
        if (cNumchars >= mcNumCharsAllowed) {
            // Safeguard just in case the DB has more chars than allowed
            break;
        }
    }
    mbCharListLoaded = true;
    LOG_DEBUG1("Character list loaded.");
}
