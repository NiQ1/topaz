/**
 *	@file CharMessageHnd.cpp
 *	Message handler for character creation / login
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "CharMessageHnd.h"
#include <mariadb++/connection.hpp>
#include "new-common/Database.h"
#include "new-common/Debugging.h"
#include "LoginGlobalConfig.h"
#include "new-common/Utilities.h"
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
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();

    LOCK_DB;
    // Check if the character exists and associated with the given content id
    std::string strSqlQueryFmt("SELECT content_id, name FROM %schars WHERE character_id=%d AND world_id=%d LIMIT 1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        pNewDetails->dwCharacterID,
        pNewDetails->cWorldID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        // Verify that the content ID exists
        strSqlQueryFmt = "SELECT * FROM %scontentss WHERE content_id=%d LIMIT 1;";
        std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->dwContentID));
        pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() == 0) {
            LOG_ERROR("Content ID does not exist.");
            throw std::runtime_error("Content ID does not exist.");
        }
        // Verify that the content is is available to use
        strSqlQueryFmt = "SELECT * FROM %schars WHERE content_id=%d LIMIT 1;";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->dwContentID);
        pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() != 0) {
            LOG_ERROR("Content ID in use by a different character.");
            throw std::runtime_error("Content ID in use by a different character.");
        }
        // Check that the name is not already taken
        strSqlQueryFmt = "SELECT FROM %schars WHERE world_id=%d and name='%s';";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->cWorldID,
            Database::RealEscapeString(pNewDetails->szCharName).c_str());
        pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() > 0) {
            LOG_ERROR("Character name already taken.");
            throw std::runtime_error("Char name already taken.");
        }
        LOG_DEBUG1("Creating new character.");
        strSqlQueryFmt = "INSERT INTO %schars (content_id, character_id, name, world_id, main_job, main_job_lv, zone, race, face, hair, head, body "
            "hands, legs, feet, main, sub, size, nation) VALUES (%d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            pNewDetails->dwContentID,
            pNewDetails->dwCharacterID,
            Database::RealEscapeString(pNewDetails->szCharName),
            pNewDetails->cWorldID,
            pNewDetails->cMainJob,
            pNewDetails->cMainJobLevel,
            pNewDetails->wZone,
            pNewDetails->cRace,
            pNewDetails->cFace,
            pNewDetails->cHair,
            pNewDetails->wBody,
            pNewDetails->wHands,
            pNewDetails->wLegs,
            pNewDetails->wFeet,
            pNewDetails->wMain,
            pNewDetails->wSub,
            pNewDetails->cSize,
            pNewDetails->cNation);
        if (DB->insert(strSqlFinalQuery) == 0) {
            LOG_ERROR("SQL insert query failed.");
            throw std::runtime_error("Insert failed.");
        }
    }
    else {
        pResultSet->next();
        uint32_t dwContentId = pResultSet->get_unsigned32(0);
        uint32_t dwWorldId = pResultSet->get_unsigned32(1);
        std::string strCharName = pResultSet->get_string(2);
        if ((dwContentId != pNewDetails->dwContentID) ||
            (dwWorldId != pNewDetails->cWorldID) ||
            (std::string(pNewDetails->szCharName) != strCharName)) {
            LOG_ERROR("Character already exists but associated with a different content id or world or has a different name.");
            throw std::runtime_error("Char/ContentID/Name mismatch.");
        }
        LOG_DEBUG1("Updating existing character.");
        strSqlQueryFmt = "UPDATE %schars SET main_job=%d, main_job_lv=%d, zone=%d, race=%d, face=%d, hair=%d "
            "head=%s, body=%s, hands=%d, legs=%d, feet=%d, main=%d, sub=%d, size=%d, nation=%d WHERE id=%d AND world_id=%d";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pNewDetails->szCharName),
            pNewDetails->cMainJob,
            pNewDetails->cMainJobLevel,
            pNewDetails->wZone,
            pNewDetails->cRace,
            pNewDetails->cFace,
            pNewDetails->cHair,
            pNewDetails->wBody,
            pNewDetails->wHands,
            pNewDetails->wLegs,
            pNewDetails->wFeet,
            pNewDetails->wMain,
            pNewDetails->wSub,
            pNewDetails->cSize,
            pNewDetails->cNation,
            pNewDetails->dwCharacterID,
            pNewDetails->cWorldID);
        if (DB->execute(strSqlFinalQuery) == 0) {
            LOG_ERROR("SQL update query failed.");
            throw std::runtime_error("Update failed.");
        }
    }
}

CHARACTER_ENTRY CharMessageHnd::QueryCharacter(uint32_t dwCharacterID, uint8_t cWorldID)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();

    LOCK_DB;
    std::string strSqlQueryFmt("SELECT content_id, name, main_job, main_job_lv, zone, race, face, hair, head, body "
        "hands, legs, feet, main, sub, size, nation FROM %schars WHERE character_id = %d AND world_id = %d LIMIT 1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID,
        cWorldID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Character does not exist.");
        throw std::runtime_error("No such character.");
    }
    pResultSet->next();
    CHARACTER_ENTRY CharEnt = { 0 };
    CharEnt.dwContentID = pResultSet->get_unsigned32(0);
    CharEnt.dwCharacterID = dwCharacterID;
    strncpy(CharEnt.szCharName, pResultSet->get_string(1).c_str(), sizeof(CharEnt.szCharName) - 1);
    CharEnt.cWorldID = cWorldID;
    CharEnt.cMainJob = static_cast<uint8_t>(pResultSet->get_unsigned32(2));
    CharEnt.cMainJobLevel = static_cast<uint8_t>(pResultSet->get_unsigned32(3));
    CharEnt.wZone = static_cast<uint16_t>(pResultSet->get_unsigned32(4));
    CharEnt.cRace = static_cast<uint8_t>(pResultSet->get_unsigned32(5));
    CharEnt.cFace = static_cast<uint8_t>(pResultSet->get_unsigned32(6));
    CharEnt.cHair = static_cast<uint8_t>(pResultSet->get_unsigned32(7));
    CharEnt.wHead = static_cast<uint16_t>(pResultSet->get_unsigned32(8));
    CharEnt.wBody = static_cast<uint16_t>(pResultSet->get_unsigned32(9));
    CharEnt.wHands = static_cast<uint16_t>(pResultSet->get_unsigned32(10));
    CharEnt.wLegs = static_cast<uint16_t>(pResultSet->get_unsigned32(11));
    CharEnt.wFeet = static_cast<uint16_t>(pResultSet->get_unsigned32(12));
    CharEnt.wMain = static_cast<uint16_t>(pResultSet->get_unsigned32(13));
    CharEnt.wSub = static_cast<uint16_t>(pResultSet->get_unsigned32(14));
    CharEnt.cSize = static_cast<uint8_t>(pResultSet->get_unsigned32(15));
    CharEnt.cNation = static_cast<uint8_t>(pResultSet->get_unsigned32(16));
    return CharEnt;
}

CHARACTER_ENTRY CharMessageHnd::QueryCharacter(uint32_t dwContentID)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();

    LOCK_DB;
    std::string strSqlQueryFmt("SELECT character_id, name, world_id, main_job, main_job_lv, zone, race, face, hair, head "
        "body, hands, legs, feet, main, sub, size, nation FROM %schars WHERE content_id = %d LIMIT 1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwContentID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Character does not exist.");
        throw std::runtime_error("No such character.");
    }
    pResultSet->next();
    CHARACTER_ENTRY CharEnt = { 0 };
    CharEnt.dwContentID = dwContentID;
    CharEnt.dwCharacterID = pResultSet->get_unsigned32(0);
    strncpy(CharEnt.szCharName, pResultSet->get_string(1).c_str(), sizeof(CharEnt.szCharName) - 1);
    CharEnt.cWorldID = static_cast<uint8_t>(pResultSet->get_unsigned32(2));
    CharEnt.cMainJob = static_cast<uint8_t>(pResultSet->get_unsigned32(3));
    CharEnt.cMainJobLevel = static_cast<uint8_t>(pResultSet->get_unsigned32(4));
    CharEnt.wZone = static_cast<uint16_t>(pResultSet->get_unsigned32(5));
    CharEnt.cRace = static_cast<uint8_t>(pResultSet->get_unsigned32(6));
    CharEnt.cFace = static_cast<uint8_t>(pResultSet->get_unsigned32(7));
    CharEnt.cHair = static_cast<uint8_t>(pResultSet->get_unsigned32(8));
    CharEnt.wHead = static_cast<uint16_t>(pResultSet->get_unsigned32(9));
    CharEnt.wBody = static_cast<uint16_t>(pResultSet->get_unsigned32(10));
    CharEnt.wHands = static_cast<uint16_t>(pResultSet->get_unsigned32(11));
    CharEnt.wLegs = static_cast<uint16_t>(pResultSet->get_unsigned32(12));
    CharEnt.wFeet = static_cast<uint16_t>(pResultSet->get_unsigned32(13));
    CharEnt.wMain = static_cast<uint16_t>(pResultSet->get_unsigned32(14));
    CharEnt.wSub = static_cast<uint16_t>(pResultSet->get_unsigned32(15));
    CharEnt.cSize = static_cast<uint8_t>(pResultSet->get_unsigned32(16));
    CharEnt.cNation = static_cast<uint8_t>(pResultSet->get_unsigned32(17));
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
    switch (eMessageType) {
    case MQConnection::MQ_MESSAGE_CHAR_LOGIN_ACK:
    case MQConnection::MQ_MESSAGE_CHAR_CREATE_ACK:
    case MQConnection::MQ_MESSAGE_CHAR_DELETE_ACK:
    case MQConnection::MQ_MESSAGE_CHAR_RESERVE_ACK:
        // These are handled by the view server so just push the message to it
        {
            LOG_DEBUG0("Pushing message to view server.");
            LOCK_TRACKER;
            std::shared_ptr<uint8_t> pMessage(new uint8_t[Request.len]);
            memcpy(pMessage.get(), Request.bytes, Request.len);
            SessionTracker::GetInstance()->GetSessionDetails(pHeader->dwAccountID)->SendMQMessageToViewServer(pMessage, pOrigin->GetWorldId());
        }
        break;
    case MQConnection::MQ_MESSAGE_CHAR_UPDATE:
        // Full character update
        LOG_DEBUG0("Updating character in DB.");
        CHARACTER_ENTRY* pCharEntry = reinterpret_cast<CHARACTER_ENTRY*>(reinterpret_cast<uint8_t*>(Request.bytes) + sizeof(CHAR_MQ_MESSAGE_HEADER));
        if ((pCharEntry->dwCharacterID != pHeader->dwCharacterID) ||
            (pCharEntry->cWorldID != pOrigin->GetWorldId())) {
            // Someone attempting to trick us into updating a different character
            LOG_ERROR("Message header / character entry ID mismatch.");
            throw std::runtime_error("Message header / character entry ID mismatch.");
        }
        UpdateCharacter(pCharEntry);
        break;
    }
    return true;
}
