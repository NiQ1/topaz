/**
 *	@file CharMessageHnd.cpp
 *	Message handler for character creation / login
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "CharMessageHnd.h"
#include <mariadb++/connection.hpp>
#include "Database.h"
#include "Debugging.h"
#include "GlobalConfig.h"
#include "Utilities.h"
#include "SessionTracker.h"

#define MAX_CHAR_MESSAGE_SIZE 1048576

CharMessageHnd::CharMessageHnd()
{
    LOG_DEBUG0("Called.");
}

CharMessageHnd::~CharMessageHnd()
{
    LOG_DEBUG0("Called.");
}

void CharMessageHnd::UpdateCharacter(CHARACTER_ENTRY* pNewDetails)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();

    LOCK_DB;
    // Check if the character exists and associated with the given account
    std::string strSqlQueryFmt("SELECT accout_id, world_id, name FROM %schars WHERE id=%d LIMIT 1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        pNewDetails->dwCharacterID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        // Check that the name is not already taken
        strSqlQueryFmt = "SELECT FROM %schars WHERE world_id=%d and name='%s';";
        std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->cWorldID,
            Database::RealEscapeString(pNewDetails->szCharName).c_str()));
        pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() > 0) {
            LOG_ERROR("Character name already taken.");
            throw std::runtime_error("Char name already taken.");
        }
        LOG_DEBUG1("Creating new character.");
        strSqlQueryFmt = "INSERT INTO %schars (id, name, account_id, world_id, main_job, main_job_lv, zone, race, face "
            "head, body, hands, legs, feet, main, sub) VALUES (%d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->dwCharacterID,
            Database::RealEscapeString(pNewDetails->szCharName),
            pNewDetails->dwAccountID,
            pNewDetails->cWorldID,
            pNewDetails->cMainJob,
            pNewDetails->cMainJobLevel,
            pNewDetails->wZone,
            pNewDetails->cRace,
            pNewDetails->wFace,
            pNewDetails->wBody,
            pNewDetails->wHands,
            pNewDetails->wLegs,
            pNewDetails->wFeet,
            pNewDetails->wMain,
            pNewDetails->wSub);
        if (DB->insert(strSqlFinalQuery) == 0) {
            LOG_ERROR("SQL insert query failed.");
            throw std::runtime_error("Insert failed.");
        }
    }
    else {
        pResultSet->next();
        uint32_t dwAccountId = pResultSet->get_unsigned32(0);
        uint32_t dwWorldId = pResultSet->get_unsigned32(1);
        std::string strCharName = pResultSet->get_string(2);
        if ((dwAccountId != pNewDetails->dwAccountID) ||
            (dwWorldId != pNewDetails->cWorldID) ||
            (std::string(pNewDetails->szCharName) != strCharName)) {
            LOG_ERROR("Character already exists but associated with a different account or world or has a different name.");
            throw std::runtime_error("Char/Account/Name mismatch.");
        }
        LOG_DEBUG1("Updating existing character.");
        strSqlQueryFmt = "UPDATE %schars SET main_job=%d, main_job_lv=%d, zone=%d, race=%d, face=%d "
            "head=%s, body=%s, hands=%d, legs=%d, feet=%d, main=%d, sub=%d WHERE id=%d";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pNewDetails->szCharName),
            pNewDetails->cMainJob,
            pNewDetails->cMainJobLevel,
            pNewDetails->wZone,
            pNewDetails->cRace,
            pNewDetails->wFace,
            pNewDetails->wBody,
            pNewDetails->wHands,
            pNewDetails->wLegs,
            pNewDetails->wFeet,
            pNewDetails->wMain,
            pNewDetails->wSub,
            pNewDetails->dwCharacterID);
        if (DB->execute(strSqlFinalQuery) == 0) {
            LOG_ERROR("SQL update query failed.");
            throw std::runtime_error("Update failed.");
        }
    }
}

CharMessageHnd::CHARACTER_ENTRY CharMessageHnd::QueryCharacter(uint32_t dwCharacterID)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();

    LOCK_DB;
    std::string strSqlQueryFmt("SELECT name, account_id, world_id, main_job, main_job_lv, zone, race, face "
        "head, body, hands, legs, feet, main, sub FROM %schars WHERE id = %d LIMIT 1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Character does not exist.");
        throw std::runtime_error("No such character.");
    }
    pResultSet->next();
    CHARACTER_ENTRY CharEnt = { 0 };
    CharEnt.dwCharacterID = dwCharacterID;
    strncpy(CharEnt.szCharName, pResultSet->get_string(0).c_str(), sizeof(CharEnt.szCharName) - 1);
    CharEnt.dwAccountID = pResultSet->get_unsigned32(1);
    CharEnt.cWorldID = static_cast<uint8_t>(pResultSet->get_unsigned32(2));
    CharEnt.cMainJob = static_cast<uint8_t>(pResultSet->get_unsigned32(3));
    CharEnt.cMainJobLevel = static_cast<uint8_t>(pResultSet->get_unsigned32(4));
    CharEnt.wZone = static_cast<uint16_t>(pResultSet->get_unsigned32(5));
    CharEnt.cRace = static_cast<uint8_t>(pResultSet->get_unsigned32(6));
    CharEnt.wFace = static_cast<uint16_t>(pResultSet->get_unsigned32(7));
    CharEnt.wHead = static_cast<uint16_t>(pResultSet->get_unsigned32(8));
    CharEnt.wBody = static_cast<uint16_t>(pResultSet->get_unsigned32(9));
    CharEnt.wHands = static_cast<uint16_t>(pResultSet->get_unsigned32(10));
    CharEnt.wLegs = static_cast<uint16_t>(pResultSet->get_unsigned32(11));
    CharEnt.wFeet = static_cast<uint16_t>(pResultSet->get_unsigned32(12));
    CharEnt.wMain = static_cast<uint16_t>(pResultSet->get_unsigned32(13));
    CharEnt.wSub = static_cast<uint16_t>(pResultSet->get_unsigned32(14));
    return CharEnt;
}

bool CharMessageHnd::HandleRequest(amqp_bytes_t Request, MQConnection* pOrigin)
{
    LOG_DEBUG0("Called.");

    if (Request.len < sizeof(MQConnection::MQ_MESSAGE_TYPES)) {
        LOG_ERROR("Received message is too small.");
        throw std::runtime_error("Message too small.");
    }
    // Sanity, don't allocate too much memory
    if (Request.len > MAX_CHAR_MESSAGE_SIZE) {
        LOG_ERROR("Message size too big.");
        throw std::runtime_error("Message too big.");
    }
    MQConnection::MQ_MESSAGE_TYPES eMessageType = *reinterpret_cast<MQConnection::MQ_MESSAGE_TYPES*>(Request.bytes);
    if ((eMessageType < MQConnection::MQ_MESSAGE_GET_ACCOUNT_CHARS) || (eMessageType > MQConnection::MQ_MESSAGE_CHAR_RESERVE_ACK)) {
        // This is not a message we're handling so pass on to next handler
        LOG_DEBUG0("Not a message for this handler, passing.");
        return false;
    }
    // This handler expects every message to begin with a fixed header containing
    // the target content id, account id etc. so make sure we have it
    if (Request.len < sizeof(CHAR_MQ_MESSAGE_HEADER)) {
        LOG_ERROR("Received message too small for character message header.");
        throw std::runtime_error("Message too small for header.");
    }
    CHAR_MQ_MESSAGE_HEADER* pHeader = reinterpret_cast<CHAR_MQ_MESSAGE_HEADER*>(Request.bytes);
    // Send to session according to account ID
    LOCK_TRACKER;
    std::shared_ptr<uint8_t> pMessage(new uint8_t[Request.len]);
    memcpy(pMessage.get(), Request.bytes, Request.len);
    SessionTracker::GetInstance()->GetSessionDetails(pHeader->dwAccountID)->SendMQMessageToViewServer(pMessage);
}
