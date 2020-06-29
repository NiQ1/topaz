#include "CharCreateHnd.h"
#include "WorldGlobalConfig.h"
#include "new-common/Debugging.h"
#include "new-common/Utilities.h"
#include <mariadb++/connection.hpp>
#include "new-common/Database.h"

#define MAX_CHAR_MESSAGE_SIZE 1048576

CharCreateHnd::CharCreateHnd()
{
    LOG_DEBUG0("Called.");
}

CharCreateHnd::~CharCreateHnd()
{
    LOG_DEBUG0("Called.");
}

void CharCreateHnd::ReserveCharacter(uint32_t dwAccountID, uint32_t dwContentID, uint32_t dwCharacterID)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = WorldGlobalConfig::GetInstance();

    LOCK_DB;
    // Check that the content ID is not already in use (the login server should
    // have already done that, as content ID is universal, but double check).
    std::string strSqlQueryFmt("SELECT * FROM %schars WHERE contentid=%d OR charid=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwContentID,
        dwCharacterID));
    mariadb::result_set_ref pCheckResultSet = DB->query(strSqlFinalQuery);
    if (pCheckResultSet->row_count() != 0) {
        LOG_ERROR("Content ID or Character ID already in use!");
        throw std::runtime_error("Content/Character ID reuse.");
    }
    // Clear up any expired reservations
    size_t i = 0;
    while (i < mvecReservedCharIDs.size()) {
        if (mvecReservedCharIDs[i].tmExpiry >= time(NULL)) {
            mvecReservedCharIDs.erase(mvecReservedCharIDs.begin() + i);
        }
        else {
            i++;
        }
    }
    // And place it to the reserved character ID list. Note that we do not insert to DB
    // in order to avoid wasting a character ID if the user cancels at the last prompt.
    mvecReservedCharIDs.push_back({ dwCharacterID, dwContentID, dwAccountID, time(NULL)+Config->GetConfigUInt("reservation_timeout") });
}

uint32_t CharCreateHnd::CreateCharacter(uint32_t dwCharacterID, const CHARACTER_ENTRY* pCharEntry)
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = WorldGlobalConfig::GetInstance();

    LOCK_DB;
    // Chop off the world ID from the character ID, because we only store
    // the unique part in the DB
    dwCharacterID &= 0xFFFF;
    // Verify that we indeed have that character ID on reservation
    size_t dwReserveLen = mvecReservedCharIDs.size();
    size_t i = 0;
    while ((i < dwReserveLen) && (mvecReservedCharIDs[i].dwCharID != dwCharacterID)) {
        i++;
    }
    if (i >= dwReserveLen) {
        LOG_ERROR("Character ID has not been reserved for a new character.");
        throw std::runtime_error("Unreserved character ID.");
    }
    // Cross verify content ID
    if (mvecReservedCharIDs[i].dwContentID != pCharEntry->dwContentID) {
        LOG_ERROR("Content ID does not match reservation.");
        throw std::runtime_error("Character ID / Content ID mismatch.");
    }
    // We need to change some of the character data so first copy it
    // because the original is assumed to be const.
    CHARACTER_ENTRY NewChar = { 0 };
    memcpy(&NewChar, pCharEntry, sizeof(NewChar));
    // Verify that the starting job is a basic job
    if ((NewChar.cMainJob < 1) || (NewChar.cMainJob > 6)) {
        LOG_ERROR("User attempted to use an advanced job as starting job.");
        throw std::runtime_error("Invalid starting job.");
    }
    // Check whether the character ID suggested by the login server is available to use
    std::string strSqlQueryFmt("SELECT * FROM %schars WHERE charid=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID));
    bool bReplaceCharID = false;
    // Character ID of zero = no suggestion ("you decide"), otherwise attempt to use the
    // suggested character ID but replace it if already taken.
    if (dwCharacterID != 0) {
        mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() != 0) {
            bReplaceCharID = true;
        }
    }
    else {
        bReplaceCharID = true;
    }
    if (bReplaceCharID) {
        // Character ID already in use so pick the next available ID
        strSqlFinalQuery = "SELET max(charid) FROM %schars;";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str());
        mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
        // Row count will definitely be one because we know the table is not empty
        pResultSet->next();
        dwCharacterID = pResultSet->get_unsigned32(0) + 1;
    }
    LOG_DEBUG1("Using character ID: %d", dwCharacterID);
    // Determine the starting zone, which is a random zone in the player's
    // chosen nation.
    if (NewChar.cNation == 0) {
        // San d'Oria
        NewChar.wZone = 0xE6 + (rand() % 3);
    }
    else if (NewChar.cNation == 1) {
        // Bastok
        NewChar.wZone = 0xEA + (rand() % 3);
    }
    else if (NewChar.cNation == 2) {
        // Windurst
        // Note: Windurst Walls (0xEF) is not allowed as a starting zone
        NewChar.wZone = 0xEE + (rand() % 3);
        if (NewChar.wZone == 0xEF) {
            // Note 2: Windurst has 4 zones so 0xF1 is still Windurst
            NewChar.wZone = 0xF1;
        }
    }
    // Insert to DB
    strSqlQueryFmt = "INSERT INTO %schars (charid, contentid, acctid, charname, pos_zone, nation) VALUES (%u, %u, %u, '%s', %u, %u);";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID,
        mvecReservedCharIDs[i].dwContentID,
        mvecReservedCharIDs[i].dwAccountID,
        Database::RealEscapeString(NewChar.szCharName).c_str(),
        NewChar.wZone,
        NewChar.cNation);
    if (DB->insert(strSqlFinalQuery) == 0) {
        LOG_ERROR("Failed to insert new character to DB!");
        throw std::runtime_error("DB insert failure (chars).");
    }
    // Character appearance
    strSqlQueryFmt = "INSERT INTO %schar_look (charid, face, race, size) VALUES (%u, %u, %u, %u);";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID,
        NewChar.cFace,
        NewChar.cRace,
        NewChar.cSize);
    if (DB->insert(strSqlFinalQuery) == 0) {
        LOG_ERROR("Failed to insert new character look to DB!");
        throw std::runtime_error("DB insert failure (look).");
    }
    strSqlQueryFmt = "INSERT INTO %schar_stats (charid, mjob) VALUES (%u, %u);";
    strSqlFinalQuery = FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID,
        NewChar.cMainJob);
    if (DB->insert(strSqlFinalQuery) == 0) {
        LOG_ERROR("Failed to insert new character job to DB!");
        throw std::runtime_error("DB insert failure (job).");
    }
    // Reservation no longer needed
    mvecReservedCharIDs.erase(mvecReservedCharIDs.begin() + i);
    return dwCharacterID;
}

void CharCreateHnd::DeleteCharacter(uint32_t dwCharacterID)
{
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = WorldGlobalConfig::GetInstance();
    LOCK_DB;
    // Chop off the world ID from the character ID, because we only store
    // the unique part in the DB
    dwCharacterID &= 0xFFFF;
    std::string strSqlQueryFmt("DELETE FROM %schars WHERE charid=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwCharacterID));
    DB->execute(strSqlFinalQuery);
}

bool CharCreateHnd::HandleRequest(amqp_bytes_t Request, MQConnection* pOrigin)
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
    CHAR_MQ_MESSAGE_HEADER* pHeader = reinterpret_cast<CHAR_MQ_MESSAGE_HEADER*>(Request.bytes);
    switch (pHeader->eType) {
    case MQConnection::MQ_MESSAGE_CHAR_RESERVE:
        LOG_DEBUG0("Reserving new character.");
        ReserveCharacter(pHeader->dwAccountID, pHeader->dwContentID, pHeader->dwCharacterID);
        MESSAGE_GENERIC_RESPONSE ReserveResponse;
        // Zero response code = success
        ReserveResponse.dwResponseCode = 0;
        memcpy(&ReserveResponse.Header, pHeader, sizeof(ReserveResponse.Header));
        pHeader->eType = MQConnection::MQ_MESSAGE_CHAR_RESERVE_ACK;
        pOrigin->Send(reinterpret_cast<uint8_t*>(&ReserveResponse), sizeof(ReserveResponse));
        break;
    case MQConnection::MQ_MESSAGE_CHAR_CREATE:
        LOG_DEBUG0("Creating new character.");
        CHARACTER_ENTRY* pCharEntry = reinterpret_cast<CHARACTER_ENTRY*>(reinterpret_cast<uint8_t*>(Request.bytes) + sizeof(CHAR_MQ_MESSAGE_HEADER));
        uint32_t dwNewCharID = CreateCharacter(pHeader->dwCharacterID, pCharEntry);
        MESSAGE_CONFIRM_CREATE_RESPONSE CreateResponse;
        CreateResponse.dwResponseCode = 0;
        memcpy(&CreateResponse.Header, pHeader, sizeof(CreateResponse.Header));
        // Character ID may have been overwritten
        CreateResponse.Header.dwCharacterID = dwNewCharID;
        CreateResponse.wZone = pCharEntry->wZone;
        pHeader->eType = MQConnection::MQ_MESSAGE_CHAR_CREATE_ACK;
        pOrigin->Send(reinterpret_cast<uint8_t*>(&CreateResponse), sizeof(CreateResponse));
        break;
    }
    // This is the only handler in World Server so we catch everything.
    return true;
}
