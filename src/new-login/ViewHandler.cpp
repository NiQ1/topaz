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
#include "GlobalData.h"
#include "Utilities.h"
#include <time.h>

#define LOCK_SESSION std::lock_guard<std::mutex> l_session(*mpSession->GetMutex())

ViewHandler::ViewHandler(std::shared_ptr<TCPConnection> connection) : ProtocolHandler(connection), mParser(connection)
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
                pRawData = mParser.ReceivePacket();
                // Nasty but needed trick to get the raw pointer
                pPacketHeader = reinterpret_cast<FFXIPacket::FFXI_PACKET_HEADER*>(&(*pRawData));
                pPayloadData = (&(*pRawData)) + sizeof(FFXIPacket::FFXI_PACKET_HEADER);

                switch (pPacketHeader->dwPacketSize) {
                case FFXIPacket::FFXI_TYPE_GET_FEATURES:
                    CheckVersionAndSendFeatures(pPayloadData);
                    break;
                case FFXIPacket::FFXI_TYPE_GET_CHARACTER_LIST:
                    SendCharacterList();
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

void ViewHandler::CheckVersionAndSendFeatures(uint8_t* pRequestPacket)
{
    LOG_DEBUG0("Called.");
    // The packet has a lot of unidentified garbage, the only thing that is of
    // interest to us is the version number, which is at offset 88
    std::string strClientVersion(reinterpret_cast<char*>(pRequestPacket + 88), 10);
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
    const LoginSession::CHARACTER_ENTRY* pCurrentChar;
    GlobalDataPtr GlobData = GlobalData::GetInstance();

    CharListPacket.dwContentIds = mpSession->GetNumCharsAllowed();
    for (uint8_t i = 0; i < cNumChars; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        CharListPacket.CharList[i].dwCharacterID = pCurrentChar->dwCharacterID;
        CharListPacket.CharList[i].dwContentID = pCurrentChar->dwCharacterID;
        CharListPacket.CharList[i].dwUnknown1 = 1;
        strncpy(CharListPacket.CharList[i].szCharacterName, pCurrentChar->szCharName, sizeof(CharListPacket.CharList[i].szCharacterName) - 1);
        strncpy(CharListPacket.CharList[i].szWorldName, GlobData->GetWorldName(pCurrentChar->cWorldID).c_str(), sizeof(CharListPacket.CharList[i].szWorldName));
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
    LOG_DEBUG1("Character list send.");
}
