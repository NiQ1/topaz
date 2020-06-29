/**
 *	@file ViewHandler.h
 *	Implementation of the view protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include <mariadb++/connection.hpp>
#include "new-common/Database.h"
#include "ViewHandler.h"
#include "new-common/Debugging.h"
#include "SessionTracker.h"
#include "LoginGlobalConfig.h"
#include "WorldManager.h"
#include "new-common/Utilities.h"
#include "Authentication.h"
#include <time.h>

// Timeout for key installation (milliseconds)
#define KEY_INSTALLATION_TIMEOUT 10000
// Timeout for character list installation by bootloader
#define CHAR_LIST_INSTALLATION_TIMEOUT 10000
// Timeout for response from world server (seconds)
#define WORLD_SERVER_REPLY_TIMEOUT 10

#define LOCK_SESSION std::lock_guard<std::recursive_mutex> l_session(*mpSession->GetMutex())

ViewHandler::ViewHandler(std::shared_ptr<TCPConnection> connection) : ProtocolHandler(connection),
    mParser(connection),
    mtmOperationTimeout(0),
    mbReceivedSendCharListClient(false),
    mbReceivedSendCharListDataSrv(false)
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
    FFXILoginPacket::FFXI_PACKET_HEADER* pPacketHeader = NULL;
    uint8_t* pPayloadData = NULL;
    LoginSession::REQUESTS_TO_VIEW_SERVER RequestFromData = LoginSession::VIEW_SERVER_IDLE;
    std::shared_ptr<uint8_t> pMessageFromMQ;

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
                pPacketHeader = reinterpret_cast<FFXILoginPacket::FFXI_PACKET_HEADER*>(pRawData.get());
                pPayloadData = pRawData.get() + sizeof(FFXILoginPacket::FFXI_PACKET_HEADER);

                switch (pPacketHeader->dwPacketType) {
                case FFXILoginPacket::FFXI_TYPE_GET_FEATURES:
                    CheckVersionAndSendFeatures(pPayloadData);
                    break;
                case FFXILoginPacket::FFXI_TYPE_GET_CHARACTER_LIST:
                    // Make sure the data server has already installed the character list,
                    // otherwise wait for it.
                    mbReceivedSendCharListClient = true;
                    if (mbReceivedSendCharListDataSrv) {
                        SendCharacterList();
                    }
                    break;
                case FFXILoginPacket::FFXI_TYPE_GET_WORLD_LIST:
                    SendWorldList();
                    break;
                case FFXILoginPacket::FFXI_TYPE_LOGIN_REQUEST:
                    HandleLoginRequest(reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPayloadData));
                    break;
                case FFXILoginPacket::FFXI_TYPE_CREATE_CHARACTER:
                    PrepareNewCharacter(reinterpret_cast<CREATE_REQUEST_PACKET*>(pPayloadData));
                    break;
                case FFXILoginPacket::FFXI_TYPE_CREATE_CHAR_CONFIRM:
                    ConfirmNewCharacter(reinterpret_cast<CONFIRM_CREATE_REQUEST_PACKET*>(pPayloadData));
                    break;
                case FFXILoginPacket::FFXI_TYPE_DELETE_CHARACTER:
                    DeleteCharacter(reinterpret_cast<DELETE_REQUEST_PACKET*>(pPayloadData));
                    break;
                default:
                    LOG_WARNING("Received an unknown packet type from client, ignoring.");
                }
            }

            // Check if the data server wants us to do something
            RequestFromData = mpSession->GetRequestFromDataServer();
            if (RequestFromData != LoginSession::VIEW_SERVER_IDLE) {
                switch (RequestFromData) {
                case LoginSession::VIEW_SERVER_SEND_CHARACTER_LIST:
                    mbReceivedSendCharListDataSrv = true;
                    if (mbReceivedSendCharListClient) {
                        // Only send the character list if the client requested it
                        SendCharacterList();
                    }
                    break;
                case LoginSession::VIEW_SERVER_PROCEED_LOGIN:
                    HandleLoginRequest(&mLastLoginRequestPacket);
                    break;
                default:
                    LOG_ERROR("View server in invalid state.");
                    throw std::runtime_error("View server in invalid state.");
                }
                mpSession->SendRequestToViewServer(LoginSession::VIEW_SERVER_IDLE);
            }

            // Maybe we have a message waiting from MQ
            uint8_t cOrigin = 0;
            pMessageFromMQ = mpSession->GetMessageFromMQ(&cOrigin);
            if (pMessageFromMQ != NULL) {
                switch (*reinterpret_cast<MQConnection::MQ_MESSAGE_TYPES*>(pMessageFromMQ.get())) {
                case MQConnection::MQ_MESSAGE_CHAR_LOGIN_ACK:
                    CompleteLoginRequest(pMessageFromMQ, cOrigin);
                    break;
                case MQConnection::MQ_MESSAGE_CHAR_RESERVE_ACK:
                    CompletePrepareNewChar(pMessageFromMQ, cOrigin);
                    break;
                case MQConnection::MQ_MESSAGE_CHAR_CREATE_ACK:
                    CompleteConfirmNewCharacter(pMessageFromMQ, cOrigin);
                    break;
                case MQConnection::MQ_MESSAGE_CHAR_DELETE_ACK:
                    CompleteDeleteCharacter(pMessageFromMQ, cOrigin);
                default:
                    LOG_ERROR("Invalid message received from world server.");
                    mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
                    throw std::runtime_error("MQ message type unknown.");
                }
            }

            // Maybe we've timed out
            if ((mtmOperationTimeout != 0) && (time(NULL) >= mtmOperationTimeout)) {
                LOG_ERROR("Timed out waiting for a reply from the world server.");
                mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
                throw std::runtime_error("World server response timeout.");
            }
        }
    }
    catch (...) {
        LOG_ERROR("Exception thown by view server, disconnecting client.");
    }

    mpSession->SetViewServerFinished();
    if (mpSession->IsDataServerFinished()) {
        mpSession->SetExpiryTimeAbsolute(0);
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
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    uint32_t dwVersionLock = Config->GetConfigUInt("version_lock");
    std::string strExpectedVersion(Config->GetConfigString("expected_client_version"));
    if ((dwVersionLock == 1) && (strClientVersion != strExpectedVersion)) {
        LOG_WARNING("Received connection from a client with a wrong version.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_VERSION_MISMATCH);
        throw std::runtime_error("Client version mismatch.");
    }
    if (dwVersionLock == 2) {
        // Only the first six digits are of relevance here
        uint32_t dwClientVer = std::stoul(strClientVersion.substr(0, 6));
        uint32_t dwExpectedVer = std::stoul(strExpectedVersion.substr(0, 6));
        if (dwClientVer < dwExpectedVer) {
            LOG_WARNING("Received connection from a client with a version too old.");
            mParser.SendError(FFXILoginPacket::FFXI_ERROR_VERSION_MISMATCH);
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
    mParser.SendPacket(FFXILoginPacket::FFXI_TYPE_FEATURES_LIST, reinterpret_cast<uint8_t*>(&ExpFeatures), sizeof(ExpFeatures));
}

void ViewHandler::SendCharacterList()
{
    LOG_DEBUG0("Called.");

    LOCK_SESSION;
    VIEW_PACKET_CHARACTER_LIST CharListPacket = { 0 };
    // Clear any previously reserved but not created characters
    CleanHalfCreatedCharacters();
    // Load character list from DB into session
    mpSession->LoadCharacterList();
    uint8_t cNumCharsAllowed = min(mpSession->GetNumCharsAllowed(), 16);
    const CHARACTER_ENTRY* pCurrentChar;
    WorldManagerPtr WorldMgr = WorldManager::GetInstance();

    CharListPacket.dwContentIds = mpSession->GetNumCharsAllowed();
    for (uint8_t i = 0; i < CharListPacket.dwContentIds; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        CharListPacket.CharList[i].dwContentID = pCurrentChar->dwContentID;
        CharListPacket.CharList[i].dwEnabled = pCurrentChar->bEnabled;
        if (pCurrentChar->szCharName[0] == ' ') {
            // This content ID is not associated with a character so don't do meaningless work
            continue;
        }
        CharListPacket.CharList[i].dwCharacterID = pCurrentChar->dwCharacterID;
        strncpy(CharListPacket.CharList[i].szCharacterName, pCurrentChar->szCharName, sizeof(CharListPacket.CharList[i].szCharacterName) - 1);
        strncpy(CharListPacket.CharList[i].szWorldName, WorldMgr->GetWorldName(pCurrentChar->cWorldID).c_str(), sizeof(CharListPacket.CharList[i].szWorldName));
        CharListPacket.CharList[i].Details.cRace = pCurrentChar->cRace;
        CharListPacket.CharList[i].Details.cMainJob = pCurrentChar->cMainJob;
        CharListPacket.CharList[i].Details.cNation = pCurrentChar->cNation;
        CharListPacket.CharList[i].Details.cSize = pCurrentChar->cSize;
        CharListPacket.CharList[i].Details.cFace = pCurrentChar->cFace;
        CharListPacket.CharList[i].Details.cHair = pCurrentChar->cHair;
        CharListPacket.CharList[i].Details.wHead = pCurrentChar->wHead;
        CharListPacket.CharList[i].Details.wBody = pCurrentChar->wBody;
        CharListPacket.CharList[i].Details.wHands = pCurrentChar->wHands;
        CharListPacket.CharList[i].Details.wLegs = pCurrentChar->wLegs;
        CharListPacket.CharList[i].Details.wFeet = pCurrentChar->wFeet;
        CharListPacket.CharList[i].Details.wMain = pCurrentChar->wMain;
        CharListPacket.CharList[i].Details.wSub = pCurrentChar->wSub;
        CharListPacket.CharList[i].Details.cZone1 = static_cast<uint8_t>(pCurrentChar->wZone);
        CharListPacket.CharList[i].Details.cMainJobLevel = pCurrentChar->cMainJobLevel;
        CharListPacket.CharList[i].Details.bufUnknown5[0] = 1;
        CharListPacket.CharList[i].Details.bufUnknown5[1] = 0;
        CharListPacket.CharList[i].Details.bufUnknown5[2] = 2;
        CharListPacket.CharList[i].Details.bufUnknown5[3] = 0;
        CharListPacket.CharList[i].Details.wZone2 = pCurrentChar->wZone;
    }
    LOG_DEBUG1("Sending character list.");
    mParser.SendPacket(FFXILoginPacket::FFXI_TYPE_CHARACTER_LIST, reinterpret_cast<uint8_t*>(&CharListPacket), sizeof(CharListPacket));
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
    mParser.SendPacket(FFXILoginPacket::FFXI_TYPE_WORLD_LIST, pWorldListPacket.get(), dwWorldListPacketSize);
    LOG_DEBUG1("World list sent.");
}

void ViewHandler::HandleLoginRequest(const LOGIN_REQUEST_PACKET* pRequestPacket)
{
    // Session key
    const uint8_t* bufKey = NULL;

    LOG_DEBUG0("Called.");
    // Backup the packet, as it will be needed in the second phase
    memcpy(&mLastLoginRequestPacket, pRequestPacket, sizeof(mLastLoginRequestPacket));

    LOCK_SESSION;
    // Notify the world server that a character wants to log in
    MESSAGE_LOGIN_REQUEST LoginMessage;
    LoginMessage.Header.eType = MQConnection::MQ_MESSAGE_CHAR_LOGIN;
    LoginMessage.Header.dwContentID = pRequestPacket->dwContentID;
    LoginMessage.Header.dwCharacterID = pRequestPacket->dwCharacterID;
    LoginMessage.Header.dwAccountID = mpSession->GetAccountID();
    memcpy(&LoginMessage.bufInitialKey, bufKey, sizeof(LoginMessage.bufInitialKey));
    LoginMessage.dwIPAddress = mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr;
    LoginMessage.dwExpansions = mpSession->GetExpansionsBitmask();
    LoginMessage.dwFeatures = mpSession->GetFeaturesBitmask();
    // The client chops the character ID to uint16 (apparently this also happens on retail)
    // so we'll need to iterate the account characters and look for the complete character ID.
    // this should work unless the user has two characters in two different worlds with the
    // same name and that somehow share the same 2 lower level bytes of their character ID
    // (extremely unlikely).
    uint8_t cNumChars = mpSession->GetNumCharacters();
    const CHARACTER_ENTRY* pCurrentChar = NULL;
    for (uint8_t i = 0; i < cNumChars; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        if (pCurrentChar == NULL) {
            LOG_ERROR("Get character returned NULL pointer.");
            throw std::runtime_error("Get character failed.");
        }
        if (!pCurrentChar->bEnabled) {
            LOG_ERROR("Attempted to login into a disabled content ID.");
            throw std::runtime_error("Content ID disabled.");
        }
        if ((pCurrentChar->dwCharacterID % 0x10000 == pRequestPacket->dwCharacterID) &&
            (pCurrentChar->dwContentID == pRequestPacket->dwContentID) &&
            (strncmp(pCurrentChar->szCharName, pRequestPacket->szCharacterName, sizeof(pCurrentChar->szCharName)) == 0)) {
            LoginMessage.Header.dwCharacterID = pCurrentChar->dwCharacterID;
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

void ViewHandler::CompleteLoginRequest(std::shared_ptr<uint8_t> pMQMessage, uint8_t cWorldID)
{
    LOG_DEBUG0("Called.");
    MESSAGE_LOGIN_RESPONSE* pResponseMessage = reinterpret_cast<MESSAGE_LOGIN_RESPONSE*>(pMQMessage.get());
    // Do some sanity on the message we got
    if ((pResponseMessage->Header.eType != MQConnection::MQ_MESSAGE_CHAR_LOGIN_ACK) ||
        (pResponseMessage->Header.dwAccountID != mpSession->GetAccountID()) ||
        (mpSession->IsContentIDAssociatedWithSession(pResponseMessage->Header.dwContentID) == 0)) {
        // From the client's point of view this is a communication error with the map server
        LOG_ERROR("Received an invalid response from the map server (Header details don't match request).");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Login response detail mismatch.");
    }
    CHARACTER_ENTRY* pCharEntry = mpSession->GetCharacterByContentID(pResponseMessage->Header.dwContentID);
    if ((pCharEntry->cWorldID != cWorldID) || (pCharEntry->dwCharacterID != pResponseMessage->Header.dwCharacterID)) {
        LOG_ERROR("Character ID does not match content ID.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Char id / content id mismatch.");
    }
    if (pResponseMessage->dwResponseCode != 0) {
        LOG_ERROR("Server rejected login request.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Login request rejected.");
    }
    // Send the map server details to client
    LOGIN_CONFIRM_PACKET ConfirmPacket = { 0 };
    ConfirmPacket.dwContentID = mLastLoginRequestPacket.dwContentID;
    ConfirmPacket.dwCharacterID = mLastLoginRequestPacket.dwCharacterID;
    strncpy(ConfirmPacket.szCharacterName, mLastLoginRequestPacket.szCharacterName, sizeof(ConfirmPacket.szCharacterName)-1);
    ConfirmPacket.dwUnknown = 2;
    ConfirmPacket.dwZoneIP = pResponseMessage->dwZoneIP;
    ConfirmPacket.wZonePort = pResponseMessage->wZonePort;
    ConfirmPacket.wZero1 = 0;
    ConfirmPacket.dwSearchIP = pResponseMessage->dwSearchIP;
    ConfirmPacket.wSearchPort = pResponseMessage->wSearchPort;
    ConfirmPacket.wZero2 = 0;
    LOG_INFO("Character %s (%d) successfully logged-in.", ConfirmPacket.szCharacterName, ConfirmPacket.dwCharacterID);
    mParser.SendPacket(FFXILoginPacket::FFXI_TYPE_LOGIN_RESPONSE, reinterpret_cast<uint8_t*>(&ConfirmPacket), sizeof(ConfirmPacket));
    // At this point the client should switch to the zone server, our job's
    // done so drop the connection.
    mbShutdown = true;
}

void ViewHandler::PrepareNewCharacter(const CREATE_REQUEST_PACKET* pRequestPacket)
{
    LOG_DEBUG0("Called.");
    // This will throw if the client attempts to use a content ID not associated with its account
    CHARACTER_ENTRY* pNewChar = mpSession->GetCharacterByContentID(pRequestPacket->dwContentID);
    // Do some sanity checks on the content
    if (!pNewChar->bEnabled) {
        LOG_ERROR("Cannot create a new character using a disabled content ID.");
        throw std::runtime_error("Content ID is disabled.");
    }
    if (pNewChar->dwCharacterID != 0) {
        LOG_ERROR("Content ID already associated with a character.");
        throw std::runtime_error("Content ID not free.");
    }
    // The client sends the world name rather than world ID so look it up
    WorldManagerPtr WorldMgr = WorldManager::GetInstance();
    uint32_t dwWorldID = WorldMgr->GetWorldIDByName(pRequestPacket->szWorldName);
    LOCK_SESSION;
    // Prevent unprivileges users from creating characters in test worlds by editing
    // the world name in memory.
    if ((WorldMgr->IsTestWorld(dwWorldID)) && ((mpSession->GetPrivilegesBitmask() & Authentication::ACCT_PRIV_HAS_TEST_ACCESS) == 0)) {
        LOG_ERROR("Unprivileged user attempted to create a character on a test world.");
        throw std::runtime_error("User cannot create characters on test worlds.");
    }
    DBConnection DB = Database::GetDatabase();
    LOCK_DB;
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    // We will need a unique character ID (for the given world), so just pick the existing
    // max charid + 1
    // Note: This is a SUGGESTED character ID and may be overwritten by the world server.
    uint32_t dwNewCharID = 0;
    std::string strSqlQueryFmt("SELECT MAX(character_id) FROM %schars WHERE world_id=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        dwWorldID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        // First character being created in this world. Seems that on retail the higher word
        // of the char ID is actually the world id, giving some sort of uniqueness.
        dwNewCharID = (dwWorldID << 16) + 1;
    }
    else {
        pResultSet->next();
        dwNewCharID = pResultSet->get_unsigned32(0) + 1;
    }
    // Save a stub (all zeros) to the database to reserve the character ID but don't do much more
    pNewChar->bEnabled = true;
    pNewChar->cFace = 0;
    pNewChar->cHair = 0;
    pNewChar->cMainJob = 0;
    pNewChar->cMainJobLevel = 0;
    pNewChar->cNation = 0;
    pNewChar->cRace = 0;
    pNewChar->cSize = 0;
    pNewChar->cWorldID = static_cast<uint8_t>(dwWorldID);
    pNewChar->dwCharacterID = dwNewCharID;
    pNewChar->dwContentID = pRequestPacket->dwContentID;
    strncpy(pNewChar->szCharName, pRequestPacket->szCharacterName, sizeof(pNewChar->szCharName));
    pNewChar->wBody = 0;
    pNewChar->wFeet = 0;
    pNewChar->wHands = 0;
    pNewChar->wHead = 0;
    pNewChar->wLegs = 0;
    pNewChar->wMain = 0;
    pNewChar->wSub = 0;
    pNewChar->wZone = 0;
    // Send a request to the world server to reserve the character ID / name.
    // We will proceed once the world server confirms (this happens asynchronically)
    LOCK_WORLDMGR;
    MESSAGE_CREATE_REQUEST CreateChar;
    CreateChar.Header.dwAccountID = mpSession->GetAccountID();
    CreateChar.Header.dwCharacterID = dwNewCharID;
    CreateChar.Header.eType = MQConnection::MQ_MESSAGE_CHAR_RESERVE;
    CreateChar.Header.dwContentID = pRequestPacket->dwContentID;
    strncpy(CreateChar.szCharName, pRequestPacket->szCharacterName, sizeof(CreateChar.szCharName) - 1);
    WorldManager::GetInstance()->SendMessageToWorld(dwWorldID, reinterpret_cast<uint8_t*>(&CreateChar), sizeof(CreateChar));
    mtmOperationTimeout = time(NULL) + WORLD_SERVER_REPLY_TIMEOUT;
}

void ViewHandler::CompletePrepareNewChar(std::shared_ptr<uint8_t> pMQMessage, uint8_t cWorldID)
{
    LOG_DEBUG0("Called.");
    MESSAGE_GENERIC_RESPONSE* pResponseMessage = reinterpret_cast<MESSAGE_GENERIC_RESPONSE*>(pMQMessage.get());
    // Do some sanity on the message we got
    if ((pResponseMessage->Header.eType != MQConnection::MQ_MESSAGE_CHAR_RESERVE_ACK) ||
        (pResponseMessage->Header.dwAccountID != mpSession->GetAccountID()) ||
        (mpSession->IsContentIDAssociatedWithSession(pResponseMessage->Header.dwContentID) == 0)) {
        // From the client's point of view this is a communication error with the map server
        LOG_ERROR("Received an invalid response from the map server (Header details don't match request).");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Prepare response detail mismatch.");
    }
    CHARACTER_ENTRY* pCharEntry = mpSession->GetCharacterByContentID(pResponseMessage->Header.dwContentID);
    if ((pCharEntry->cWorldID != cWorldID) || (pCharEntry->dwCharacterID != pResponseMessage->Header.dwCharacterID)) {
        LOG_ERROR("Character ID does not match content ID.");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Char id / content id mismatch.");
    }
    if (pResponseMessage->dwResponseCode != 0) {
        LOG_ERROR("Server rejected reserve request.");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Reserve request rejected.");
    }
    mParser.SendDone();
    mtmOperationTimeout = 0;
}

void ViewHandler::ConfirmNewCharacter(const CONFIRM_CREATE_REQUEST_PACKET* pRequestPacket)
{
    LOG_DEBUG0("Called.");

    CHARACTER_ENTRY* pNewChar = mpSession->GetCharacterByContentID(pRequestPacket->dwContentID);
    if ((!pNewChar->bEnabled) || (pNewChar->cNation != 0)) {
        LOG_ERROR("Character slot invalid or already taken.");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Invalid character slot.");
    }
    // Fill in all character data. The gear data is purely used for login screen visuals so we
    // don't mind taking it from whatever the client sent us (won't affect other people).
    pNewChar->cFace = pRequestPacket->Details.cFace;
    pNewChar->cHair = pRequestPacket->Details.cHair;
    pNewChar->cSize = pRequestPacket->Details.cSize;
    pNewChar->wBody = pRequestPacket->Details.wBody;
    pNewChar->wFeet = pRequestPacket->Details.wFeet;
    pNewChar->wHands = pRequestPacket->Details.wHands;
    pNewChar->wHead = pRequestPacket->Details.wHead;
    pNewChar->wLegs = pRequestPacket->Details.wLegs;
    pNewChar->wMain = pRequestPacket->Details.wMain;
    pNewChar->wSub = pRequestPacket->Details.wSub;
    if ((pRequestPacket->Details.cMainJob < 1) || (pRequestPacket->Details.cMainJob > 6)) {
        // Prevent the user from using packet injection in order to select an advanced
        // job as the starting job.
        LOG_WARNING("User attempted to select a non-basic job as a start job.");
        // Just force is to a default value (WAR)
        pNewChar->cMainJob = 1;
    }
    else {
        pNewChar->cMainJob = pRequestPacket->Details.cMainJob;
    }
    // We don't care about what level you say you are, you always start as level 1
    pNewChar->cMainJobLevel = 1;
    pNewChar->cRace = pRequestPacket->Details.cRace;
    pNewChar->cNation = pRequestPacket->Details.cNation;
    // 0 == no zone (character just been created)
    pNewChar->wZone = 0;
    MESSAGE_CONFIRM_CREATE_REQUEST ConfirmRequest;
    ConfirmRequest.Header.eType = MQConnection::MQ_MESSAGE_CHAR_CREATE;
    ConfirmRequest.Header.dwAccountID = mpSession->GetAccountID();
    ConfirmRequest.Header.dwContentID = pNewChar->dwContentID;
    ConfirmRequest.Header.dwCharacterID = pNewChar->dwCharacterID;
    // Get the world server's confirmation before committing to DB
    LOCK_WORLDMGR;
    WorldManager::GetInstance()->SendMessageToWorld(pNewChar->cWorldID, reinterpret_cast<uint8_t*>(&ConfirmRequest), sizeof(ConfirmRequest));
    mtmOperationTimeout = time(NULL) + WORLD_SERVER_REPLY_TIMEOUT;
}

void ViewHandler::CompleteConfirmNewCharacter(std::shared_ptr<uint8_t> pMQMessage, uint8_t cWorldID)
{
    LOG_DEBUG0("Called.");
    MESSAGE_CONFIRM_CREATE_RESPONSE* pResponseMessage = reinterpret_cast<MESSAGE_CONFIRM_CREATE_RESPONSE*>(pMQMessage.get());
    // Do some sanity on the message we got
    if ((pResponseMessage->Header.eType != MQConnection::MQ_MESSAGE_CHAR_CREATE_ACK) ||
        (pResponseMessage->Header.dwAccountID != mpSession->GetAccountID()) ||
        (mpSession->IsContentIDAssociatedWithSession(pResponseMessage->Header.dwContentID) == 0)) {
        // From the client's point of view this is a communication error with the map server
        LOG_ERROR("Received an invalid response from the map server (Header details don't match request).");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Confirm response detail mismatch.");
    }
    CHARACTER_ENTRY* pCharEntry = mpSession->GetCharacterByContentID(pResponseMessage->Header.dwContentID);
    if ((pCharEntry->cWorldID != cWorldID) || (pCharEntry->dwCharacterID != pResponseMessage->Header.dwCharacterID)) {
        LOG_ERROR("Character ID does not match content ID.");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Char id / content id mismatch.");
    }
    if (pResponseMessage->dwResponseCode != 0) {
        LOG_ERROR("Server rejected confirm request.");
        CleanHalfCreatedCharacters();
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Confirm request rejected.");
    }
    // Commit the new character details to DB
    CHARACTER_ENTRY* pNewChar = mpSession->GetCharacterByContentID(pResponseMessage->Header.dwContentID);
    // World server may have overwritten the character ID
    pNewChar->dwCharacterID = pResponseMessage->Header.dwCharacterID;
    CharMessageHnd::UpdateCharacter(pNewChar);
    // Note: Successful completion of the creation process does not auto-login the user,
    // the client will request an updated character list and will then issue a login command.
    mParser.SendDone();
    mtmOperationTimeout = 0;
}

void ViewHandler::DeleteCharacter(const DELETE_REQUEST_PACKET* pRequestPacket)
{
    LOG_DEBUG0("Called.");
    CHARACTER_ENTRY* pDelChar = mpSession->GetCharacterByContentID(pRequestPacket->dwContentID);
    if (pDelChar->dwCharacterID != pRequestPacket->dwCharacterID) {
        LOG_ERROR("Character ID / Content ID mismatch.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Character ID / Content ID mismatch.");
    }
    // Basically just pass the request to the world server. We'll only do the deletion
    // once the world server confirms.
    // Note: Using CHAR_MQ_MESSAGE_HEADER because it has no arguments other than the header
    CHAR_MQ_MESSAGE_HEADER DeleteRequest;
    DeleteRequest.eType = MQConnection::MQ_MESSAGE_CHAR_DELETE;
    DeleteRequest.dwAccountID = mpSession->GetAccountID();
    DeleteRequest.dwContentID = pRequestPacket->dwContentID;
    DeleteRequest.dwCharacterID = pDelChar->dwCharacterID;
    LOCK_WORLDMGR;
    WorldManager::GetInstance()->SendMessageToWorld(pDelChar->cWorldID, reinterpret_cast<uint8_t*>(&DeleteRequest), sizeof(DeleteRequest));
    mtmOperationTimeout = time(NULL) + WORLD_SERVER_REPLY_TIMEOUT;
}

void ViewHandler::CompleteDeleteCharacter(std::shared_ptr<uint8_t> pMQMessage, uint8_t cWorldID)
{
    LOG_DEBUG0("Called.");
    MESSAGE_GENERIC_RESPONSE* pResponseMessage = reinterpret_cast<MESSAGE_GENERIC_RESPONSE*>(pMQMessage.get());
    // Do some sanity on the message we got
    if ((pResponseMessage->Header.eType != MQConnection::MQ_MESSAGE_CHAR_DELETE_ACK) ||
        (pResponseMessage->Header.dwAccountID != mpSession->GetAccountID()) ||
        (mpSession->IsContentIDAssociatedWithSession(pResponseMessage->Header.dwContentID) == 0)) {
        // From the client's point of view this is a communication error with the map server
        LOG_ERROR("Received an invalid response from the map server (Header details don't match request).");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Delete response detail mismatch.");
    }
    CHARACTER_ENTRY* pCharEntry = mpSession->GetCharacterByContentID(pResponseMessage->Header.dwContentID);
    if ((pCharEntry->cWorldID != cWorldID) || (pCharEntry->dwCharacterID != pResponseMessage->Header.dwCharacterID)) {
        LOG_ERROR("Character ID does not match content ID.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Char id / content id mismatch.");
    }
    if (pResponseMessage->dwResponseCode != 0) {
        LOG_ERROR("Server rejected delete request.");
        mParser.SendError(FFXILoginPacket::FFXI_ERROR_MAP_CONNECT_FAILED);
        throw std::runtime_error("Delete request rejected.");
    }
    // Remove the character from DB and session
    LOCK_DB;
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    DBConnection DB = Database::GetDatabase();
    std::string strSqlQueryFmt("DELETE FROM %schars WHERE content_id=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        pResponseMessage->Header.dwContentID));
    DB->execute(strSqlFinalQuery);
    // Also remove from session so the client will see this entry as free on the next character list update
    pCharEntry->cFace = 0;
    pCharEntry->cHair = 0;
    pCharEntry->cMainJob = 0;
    pCharEntry->cMainJobLevel = 0;
    pCharEntry->cNation = 0;
    pCharEntry->cRace = 0;
    pCharEntry->cSize = 0;
    pCharEntry->cWorldID = 0;
    pCharEntry->dwCharacterID = 0;
    memset(pCharEntry->szCharName, 0, sizeof(pCharEntry->szCharName));
    pCharEntry->szCharName[0] = ' ';
    pCharEntry->wBody = 0;
    pCharEntry->wFeet = 0;
    pCharEntry->wHands = 0;
    pCharEntry->wHead = 0;
    pCharEntry->wLegs = 0;
    pCharEntry->wMain = 0;
    pCharEntry->wSub = 0;
    pCharEntry->wZone = 0;
    mParser.SendDone();
    mtmOperationTimeout = 0;
}

void ViewHandler::CleanHalfCreatedCharacters()
{
    LOG_DEBUG0("Called.");
    LOCK_DB;
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    DBConnection DB = Database::GetDatabase();
    std::string strSqlQueryFmt("DELETE FROM %schars WHERE nation=0 AND content_id IN (SELECT content_id FROM %scontents WHERE account_id=%d);");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mpSession->GetAccountID()));
    DB->execute(strSqlFinalQuery);
}
