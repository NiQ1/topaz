/**
 *	@file ViewHandler.h
 *	Implementation of the view protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include <mariadb++/connection.hpp>
#include "Database.h"
#include "ViewHandler.h"
#include "Debugging.h"
#include "SessionTracker.h"
#include "GlobalConfig.h"
#include "WorldManager.h"
#include "Utilities.h"
#include "Authentication.h"
#include <time.h>

// Timeout for key installation (milliseconds)
#define KEY_INSTALLATION_TIMEOUT 10000
// Timeout for response from world server (seconds)
#define WORLD_SERVER_REPLY_TIMEOUT 10

#define LOCK_SESSION std::lock_guard<std::recursive_mutex> l_session(*mpSession->GetMutex())

ViewHandler::ViewHandler(std::shared_ptr<TCPConnection> connection) : ProtocolHandler(connection), mParser(connection), mtmOperationTimeout(0)
{
    LOG_DEBUG0("Called.");
}

ViewHandler::~ViewHandler()
{
    LOG_DEBUG0("Called.");
}

void ViewHandler::Run()
{
    LOG_DEBUG0("Called.");

    // The account ID is not sent on the view port, which is very unfortunate
    // we have to fall back to the client's IP address and hope two people
    // don't connect too quickly.
    auto pSessionTracker = SessionTracker::GetInstance();
    try {
        mpSession = pSessionTracker->LookupSessionByIP(mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr);
    }
    catch (...) {
        LOG_WARNING("Unknown user attempted to connect to view port.");
        mpConnection->Close();
        return;
    }
    // Don't catch this session again when performing IP lookups
    mpSession->SetIgnoreIPLookupFlag(true);
    // Add more time to the user, if creating a new character they may stay
    // connected to the view server for a while.
    mpSession->SetExpiryTimeRelative(600);

    // Packet pointers
    std::shared_ptr<uint8_t> pRawData;
    FFXIPacket::FFXI_PACKET_HEADER* pPacketHeader = NULL;
    uint8_t* pPayloadData = NULL;
    try {
        while (mbShutdown == false) {

            // Do we have a packet from the client?
            if (mpConnection->CanRead(1000)) {
                try {
                    pRawData = mParser.ReceivePacket();
                }
                catch (std::runtime_error) {
                    LOG_INFO("Connection closed.");
                    break;
                }
                // Nasty but needed trick to get the raw pointer
                pPacketHeader = reinterpret_cast<FFXIPacket::FFXI_PACKET_HEADER*>(pRawData.get());
                pPayloadData = pRawData.get() + sizeof(FFXIPacket::FFXI_PACKET_HEADER);

                switch (pPacketHeader->dwPacketType) {
                case FFXIPacket::FFXI_TYPE_GET_FEATURES:
                    CheckVersionAndSendFeatures(pPayloadData);
                    break;
                case FFXIPacket::FFXI_TYPE_GET_CHARACTER_LIST:
                    SendCharacterList();
                    break;
                case FFXIPacket::FFXI_TYPE_GET_WORLD_LIST:
                    SendWorldList();
                    break;
                case FFXIPacket::FFXI_TYPE_LOGIN_REQUEST:
                    HandleLoginRequest(reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPayloadData));
                    break;
                }
            }
        }
    }
    catch (...) {
        LOG_ERROR("Exception thown by view server, disconnecting client.");
    }

    mpConnection->Close();
}

void ViewHandler::CheckVersionAndSendFeatures(const uint8_t* pRequestPacket)
{
    LOG_DEBUG0("Called.");
    // The packet has a lot of unidentified garbage, the only thing that is of
    // interest to us is the version number, which is at offset 88
    std::string strClientVersion(reinterpret_cast<const char*>(pRequestPacket + 88), 10);
    LOG_DEBUG1("Client version: %s", strClientVersion.c_str());
    GlobalConfigPtr Config = GlobalConfig::GetInstance();
    uint32_t dwVersionLock = Config->GetConfigUInt("version_lock");
    std::string strExpectedVersion(Config->GetConfigString("expected_client_version"));
    if ((dwVersionLock == 1) && (strClientVersion != strExpectedVersion)) {
        LOG_WARNING("Received connection from a client with a wrong version.");
        mParser.SendError(FFXIPacket::FFXI_ERROR_VERSION_MISMATCH);
        throw std::runtime_error("Client version mismatch.");
    }
    if (dwVersionLock == 2) {
        // Only the first six digits are of relevance here
        uint32_t dwClientVer = std::stoul(strClientVersion.substr(0, 6));
        uint32_t dwExpectedVer = std::stoul(strExpectedVersion.substr(0, 6));
        if (dwClientVer < dwExpectedVer) {
            LOG_WARNING("Received connection from a client with a version too old.");
            mParser.SendError(FFXIPacket::FFXI_ERROR_VERSION_MISMATCH);
            throw std::runtime_error("Client too old.");
        }
    }

    LOG_DEBUG0("Fetching expansion and features.");
    DBConnection DB = Database::GetDatabase();
    LOCK_DB;
    LOCK_SESSION;
    mpSession->SetClientVersion(strClientVersion);
    // Pull features and expansions bitmask from DB
    std::string strSqlQueryFmt("SELECT expansions, features FROM %saccounts WHERE id=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt, 
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mpSession->GetAccountID()));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Query for expansions and features failed.");
        throw std::runtime_error("DB query failed.");
    }
    pResultSet->next();
    VIEW_PACKET_EXPANSION_AND_FEATURES ExpFeatures;
    ExpFeatures.dwExpansions = pResultSet->get_unsigned32(0);
    ExpFeatures.dwFeatures = pResultSet->get_unsigned32(1);
    LOG_DEBUG1("Expansions=0x%04X, Features=0x%04X.", ExpFeatures.dwExpansions, ExpFeatures.dwFeatures);
    // No documentation on what this means
    ExpFeatures.dwUnknown = 0xAD5DE04F;
    // Also save the data to session because we'll need to send it to MQ later on
    mpSession->SetExpansionsBitmask(ExpFeatures.dwExpansions);
    mpSession->SetFeaturesBitmask(ExpFeatures.dwFeatures);
    mParser.SendPacket(FFXIPacket::FFXI_TYPE_FEATURES_LIST, reinterpret_cast<uint8_t*>(&ExpFeatures), sizeof(ExpFeatures));
}

void ViewHandler::SendCharacterList()
{
    LOG_DEBUG0("Called.");
    LOCK_SESSION;

    VIEW_PACKET_CHARACTER_LIST CharListPacket = { 0 };
    // Load character list from DB into session
    mpSession->LoadCharacterList();
    uint8_t cNumCharsAllowed = min(mpSession->GetNumCharsAllowed(), 16);
    uint8_t cNumChars = min(mpSession->GetNumCharacters(), cNumCharsAllowed);
    const CharMessageHnd::CHARACTER_ENTRY* pCurrentChar;
    WorldManagerPtr WorldMgr = WorldManager::GetInstance();

    CharListPacket.dwContentIds = mpSession->GetNumCharsAllowed();
    // Set the enabled bit on the allowed slots so the client knows it can
    // use these content IDs.
    for (uint8_t i = 0; i < cNumCharsAllowed; i++) {
        CharListPacket.CharList[i].dwEnabled = 1;
        // Charname is a space == character doesn't exist
        // (Will be overwritten later for characters that do exist).
        CharListPacket.CharList[i].szCharacterName[0] = ' ';
    }
    // Max content IDs per account is 16, so this guarantees that no two accounts
    // will have overlapping content ids.
    uint32_t dwBaseContentID = mpSession->GetAccountID() * 16;
    for (uint8_t i = 0; i < cNumChars; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        CharListPacket.CharList[i].dwContentID = dwBaseContentID + i;
        CharListPacket.CharList[i].dwCharacterID = pCurrentChar->dwCharacterID;
        CharListPacket.CharList[i].dwEnabled = 1;
        strncpy(CharListPacket.CharList[i].szCharacterName, pCurrentChar->szCharName, sizeof(CharListPacket.CharList[i].szCharacterName) - 1);
        strncpy(CharListPacket.CharList[i].szWorldName, WorldMgr->GetWorldName(pCurrentChar->cWorldID).c_str(), sizeof(CharListPacket.CharList[i].szWorldName));
        CharListPacket.CharList[i].cRace = pCurrentChar->cRace;
        CharListPacket.CharList[i].cMainJob = pCurrentChar->cMainJob;
        CharListPacket.CharList[i].cFace = static_cast<uint8_t>(pCurrentChar->wFace);
        CharListPacket.CharList[i].cUnknown4 = 2;
        CharListPacket.CharList[i].wHead = pCurrentChar->wHead;
        CharListPacket.CharList[i].wBody = pCurrentChar->wBody;
        CharListPacket.CharList[i].wHands = pCurrentChar->wHands;
        CharListPacket.CharList[i].wLegs = pCurrentChar->wLegs;
        CharListPacket.CharList[i].wFeet = pCurrentChar->wFeet;
        CharListPacket.CharList[i].wMain = pCurrentChar->wMain;
        CharListPacket.CharList[i].wSub = pCurrentChar->wSub;
        CharListPacket.CharList[i].cZone1 = static_cast<uint8_t>(pCurrentChar->wZone);
        CharListPacket.CharList[i].cMainJobLevel = pCurrentChar->cMainJobLevel;
        CharListPacket.CharList[i].bufUnknown5[0] = 1;
        CharListPacket.CharList[i].bufUnknown5[1] = 0;
        CharListPacket.CharList[i].bufUnknown5[2] = 2;
        CharListPacket.CharList[i].bufUnknown5[3] = 0;
        CharListPacket.CharList[i].wZone2 = pCurrentChar->wZone;
    }
    LOG_DEBUG1("Sending character list.");
    mParser.SendPacket(FFXIPacket::FFXI_TYPE_CHARACTER_LIST, reinterpret_cast<uint8_t*>(&CharListPacket), sizeof(CharListPacket));
    LOG_DEBUG1("Character list sent.");
}

void ViewHandler::SendWorldList()
{
    LOG_DEBUG0("Called.");
    WorldManagerPtr WorldMgr = WorldManager::GetInstance();

    std::shared_ptr<uint8_t> pWorldListPacket;
    uint32_t dwWorldListPacketSize = 0;

    if ((mpSession->GetPrivilegesBitmask() & Authentication::ACCT_PRIV_HAS_TEST_ACCESS) != 0) {
        // User has test server access so they get the admin packet
        LOG_DEBUG1("User has test server access.");
        pWorldListPacket = WorldMgr->GetAdminWorldsPacket();
        dwWorldListPacketSize = WorldMgr->GetAdminWorldsPacketSize();
    }
    else {
        // Just a regular boring user
        LOG_DEBUG1("User does not have test server access.");
        pWorldListPacket = WorldMgr->GetUserWorldsPacket();
        dwWorldListPacketSize = WorldMgr->GetUserWorldsPacketSize();
    }
    LOG_DEBUG1("Sending world list.");
    mParser.SendPacket(FFXIPacket::FFXI_TYPE_WORLD_LIST, pWorldListPacket.get(), dwWorldListPacketSize);
    LOG_DEBUG1("World list sent.");
}

void ViewHandler::HandleLoginRequest(const LOGIN_REQUEST_PACKET* pRequestPacket)
{
    // Retry count for key installation
    uint32_t dwKeyRetryCount = 0;
    // Session key
    const uint8_t* bufKey = NULL;

    // Backup the packet, as it will be needed in the second phase
    memcpy(&mLastLoginRequestPacket, pRequestPacket, sizeof(mLastLoginRequestPacket));

    // Wait for key installation if needed
    while ((bufKey == NULL) && (dwKeyRetryCount < KEY_INSTALLATION_TIMEOUT) && (mbShutdown == false)) {
        try {
            bufKey = mpSession->GetKey();
        }
        catch (std::runtime_error) {
            dwKeyRetryCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (bufKey == NULL) {
        // Timed out waiting for client key
        // TODO: Check whether there's a more suitable error code.
        mParser.SendError(FFXIPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
    }
    LOCK_SESSION;
    // Notify the world server that a character wants to log in
    CharMessageHnd::MESSAGE_LOGIN_REQUEST LoginMessage;
    LoginMessage.eType = MQConnection::MQ_MESSAGE_CHAR_LOGIN;
    LoginMessage.dwCharacterID = pRequestPacket->dwCharacterID;
    memcpy(&LoginMessage.bufInitialKey, bufKey, sizeof(LoginMessage.bufInitialKey));
    LoginMessage.dwAccountID = mpSession->GetAccountID();
    LoginMessage.dwExpansions = mpSession->GetExpansionsBitmask();
    LoginMessage.dwFeatures = mpSession->GetFeaturesBitmask();
    // The client chops the character ID to uint16 (apparently this also happens on retail)
    // so we'll need to iterate the account characters and look for the complete character ID.
    // this should work unless the user has two characters in two different worlds with the
    // same name and that somehow share the same 2 lower level bytes of their character ID
    // (extremely unlikely).
    uint8_t cNumChars = mpSession->GetNumCharacters();
    const CharMessageHnd::CHARACTER_ENTRY* pCurrentChar = NULL;
    for (uint8_t i = 0; i < cNumChars; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        if (pCurrentChar == NULL) {
            LOG_ERROR("Get character returned NULL pointer.");
            throw std::runtime_error("Get character failed.");
        }
        if ((pCurrentChar->dwCharacterID % 0x10000 == pRequestPacket->dwCharacterID) &&
            (strncmp(pCurrentChar->szCharName, pRequestPacket->szCharacterName, sizeof(pCurrentChar->szCharName)) == 0)) {
            LoginMessage.dwCharacterID = pCurrentChar->dwCharacterID;
            break;
        }
    }
    LOCK_WORLDMGR;
    WorldManager::GetInstance()->SendMessageToWorld(pCurrentChar->cWorldID, reinterpret_cast<uint8_t*>(&LoginMessage), sizeof(LoginMessage));
    // We're stopping here and waiting for the world server to reply through the MQ, which will
    // continue the login process. Set a timeout so if the world server is down we don't keep
    // the client waiting forever.
    mtmOperationTimeout = time(NULL) + WORLD_SERVER_REPLY_TIMEOUT;
}
