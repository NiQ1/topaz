/**
 *	@file DataHandler.cpp
 *	Implementation of the data protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include <mariadb++/connection.hpp>
#include "DataHandler.h"
#include "Debugging.h"
#include "Database.h"
#include "SessionTracker.h"
#include "Utilities.h"
#include "GlobalConfig.h"
#include <time.h>
#include <stdexcept>
#include <memory>

DataHandler::DataHandler(std::shared_ptr<TCPConnection> connection) : ProtocolHandler(connection), mdwAccountID(0)
{
    LOG_DEBUG0("Called.");
}

DataHandler::~DataHandler()
{
    LOG_DEBUG0("Called.");
}

void DataHandler::Run()
{
    // Outgoing request packets are a single byte in size
    uint8_t cOutgoingBytePacket = 0;
    // Type of the incoming packet
    uint8_t cIncomingPacketType = 0;
    // Have we already received an account ID?
    bool bGotAccountID = false;
    // Have we already received a key?
    bool bGotKey = false;
    // Error has occurred
    bool bError = false;
    // Session data from the session tracker
    SessionTracker::SESSION_DATA SessionData;

    LOG_DEBUG0("Called.");
    mbRunning = true;

    // When the client connects, immediately ask for account ID
    cOutgoingBytePacket = static_cast<uint8_t>(S2C_PACKET_SEND_ACCOUNT_ID);
    if (mpConnection->WriteAll(&cOutgoingBytePacket, sizeof(cOutgoingBytePacket)) != sizeof(cOutgoingBytePacket)) {
        // Shouldn't really happen unless the client immediately dropped the connection
        LOG_WARNING("Connection dropped before account ID request was sent.");
        mpConnection->Close();
        return;
    }

    while ((mbShutdown == false) && (bError == false)) {
        // Check for response from the client
        if (!mpConnection->CanRead(1000)) {
            // No data from client, keep on waiting
            continue;
        }
        // First byte indicates which type of packet it is
        if (mpConnection->Read(&cIncomingPacketType, sizeof(cIncomingPacketType)) != sizeof(cIncomingPacketType)) {
            LOG_WARNING("Client dropped the connection.");
            bError = true;
            break;
        }
        LOG_DEBUG1("Received data from client, packet type=0x%02X", cIncomingPacketType);
        if ((!bGotAccountID) && (static_cast<CLIENT_TO_SERVER_PACKET_TYPES>(cIncomingPacketType) != C2S_PACKET_ACCOUNT_ID)) {
            // We're not willing to do anything before the client identifies itself, so this is an error.
            LOG_WARNING("Client sent data before sending its account ID, dropping connection.");
            bError = true;
            break;
        }
        switch (static_cast<CLIENT_TO_SERVER_PACKET_TYPES>(cIncomingPacketType)) {
        case C2S_PACKET_ACCOUNT_ID:
            // Packet is 2 32-bit uints with account ID and server address
            ACCOUNT_ID_PACKET AccountPacket;
            if (mpConnection->ReadAll(reinterpret_cast<uint8_t*>(&AccountPacket), sizeof(AccountPacket)) != sizeof(AccountPacket)) {
                LOG_WARNING("Client sent an incomplete account ID packet.");
                bError = true;
                break;
            }
            LOG_DEBUG1("Client claims account ID: %d", AccountPacket.dwAccountID);
            // Verify we have it in the session tracker (meaning it passed through the authentication server)
            try {
                SessionData = SessionTracker::GetInstance()->GetSessionDetails(AccountPacket.dwAccountID);
            }
            catch (std::runtime_error) {
                LOG_WARNING("Client tried to connect to data server before authenticating.");
                bError = true;
                break;
            }
            // Also verify that it's the same client that authenticated and that the session has not expired
            if (SessionData.dwIpAddr != mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr) {
                LOG_WARNING("Account ID / IP address mismatch.");
                bError = true;
                break;
            }
            if (SessionData.tmExpires <= time(NULL)) {
                LOG_WARNING("Client session has expired.");
                bError = true;
                break;
            }
            LOG_DEBUG1("Account ID check passed.");
            // Client passed account ID check
            mdwAccountID = AccountPacket.dwAccountID;
            bGotAccountID = true;
            break;
        case C2S_PACKET_KEY:
            // Packet is a 24-byte key. Add to session data. Note - Since we only accept this packet after
            // the account ID packet has been processed, we know that SessionData is already initialied so
            // no need to do that again.
            if (mpConnection->ReadAll(SessionData.bufInitialKey, sizeof(SessionData.bufInitialKey)) != sizeof(SessionData.bufInitialKey)) {
                LOG_WARNING("Client sent an incomplete key packet.");
                bError = true;
                break;
            }
            // Update the key in the session tracker, also give the client more session time because the
            // session needs to stay alive until the client actually logs into the game, which may take
            // a while if s/he creating a new character.
            LOG_DEBUG1("Receving key from client.");
            SessionData.tmExpires = time(NULL) + 600;
            SessionTracker::GetInstance()->SetSessionDetails(SessionData);
            bGotKey = true;
            LOG_DEBUG1("Key updated.");
            break;
        default:
            LOG_WARNING("Client sent an unrecognized packet type.");
            bError = true;
        }

        // Don't answer on error, drop the connection immediately
        if (bError) {
            break;
        }
        // Determine what our answer is going to be. If we arrived here we know that the
        // client has already provided its account ID so no need to check that
        if (bGotKey == false) {
            // We still need the key
            cOutgoingBytePacket = static_cast<uint8_t>(S2C_PACKET_SEND_KEY);
            if (mpConnection->WriteAll(&cOutgoingBytePacket, sizeof(cOutgoingBytePacket)) != sizeof(cOutgoingBytePacket)) {
                LOG_WARNING("Client dropped the connection.");
                break;
            }
        }
        else {
            LOG_DEBUG1("Sending character list to client.");
            SendCharacterList();
        }
    }
    LOG_INFO("Client successfully connected with account ID: %d", mdwAccountID);
    mpConnection->Close();
    mbRunning = false;
}

void DataHandler::SendCharacterList()
{
    LOG_DEBUG0("Called.");
    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();

    // The size of the packet is determined by the number of characters
    // the user is allowed to create, which is the content_ids column
    // in the accounts table.
    std::string strSqlQueryFmt("SELECT content_ids FROM %saccounts WHERE id=%d;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
        mdwAccountID));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Failed to query the number of content IDs.");
        throw std::runtime_error("content_ids query failed.");
    }
    pResultSet->next();
    uint8_t cContentIds = pResultSet->get_unsigned8(0);
    // So now we know the size of the packet we're sending
    CHARACTER_ENTRY* pCharList = new CHARACTER_ENTRY[cContentIds];
    try {
        memset(pCharList, 0, sizeof(CHARACTER_ENTRY) * cContentIds);
        strSqlQueryFmt = "SELECT id FROM %schars WHERE account_id=%d;";
        strSqlFinalQuery = FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            mdwAccountID);
        pResultSet = DB->query(strSqlFinalQuery);
        if (pResultSet->row_count() == 0) {
            LOG_ERROR("Failed to query the list of characters.");
            throw std::runtime_error("char ids query failed.");
        }
        uint8_t cNumchars = 0;
        uint32_t dwCurrentChar = 0;
        while (pResultSet->next()) {
            dwCurrentChar = pResultSet->get_unsigned32(0);
            pCharList[cNumchars].dwContentID = dwCurrentChar;
            pCharList[cNumchars].dwCharacterID = dwCurrentChar;
            cNumchars++;
            if (cNumchars >= cContentIds) {
                // Safeguard just in case the DB has more chars than allowed
                break;
            }
        }
        // The header is the packet type and number of chars, and it actually
        // overwrites the first two bytes, but since they're guaranteed to be
        // zero anyway, that's fine.
        reinterpret_cast<uint8_t*>(pCharList)[0] = static_cast<uint8_t>(S2C_PACKET_CHARACTER_LIST);
        reinterpret_cast<uint8_t*>(pCharList)[1] = cNumchars;
        LOG_DEBUG1("Sending character list.");
        if (mpConnection->WriteAll(reinterpret_cast<uint8_t*>(pCharList), sizeof(CHARACTER_ENTRY) * cContentIds) <= 0) {
            LOG_ERROR("Connection error when sending character ID list.");
            throw std::runtime_error("Connection dropped.");
        }
        LOG_DEBUG1("Character list send.");
    }
    catch (...) {
        delete pCharList;
        throw;
    }
    delete pCharList;
}
