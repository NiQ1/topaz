/**
 *	@file DataHandler.cpp
 *	Implementation of the data protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include <mariadb++/connection.hpp>
#include "DataHandler.h"
#include "new-common/Debugging.h"
#include "new-common/Database.h"
#include "SessionTracker.h"
#include "new-common/Utilities.h"
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
    // Request from view server
    LoginSession::REQUESTS_TO_DATA_SERVER RequestFromView;

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

        // Maybe we have a request from the view server
        RequestFromView = mpSession->GetRequestFromViewServer();
        if (RequestFromView != LoginSession::DATA_SERVER_IDLE) {
            if (RequestFromView == LoginSession::DATA_SERVER_ASK_FOR_KEY) {
                cOutgoingBytePacket = static_cast<uint8_t>(S2C_PACKET_SEND_KEY);
                if (mpConnection->WriteAll(&cOutgoingBytePacket, sizeof(cOutgoingBytePacket)) != sizeof(cOutgoingBytePacket)) {
                    LOG_WARNING("Client dropped the connection.");
                    break;
                }
            }
            else {
                LOG_ERROR("Unknown data server state.");
                break;
            }
            // Clear our own state machine
            mpSession->SendRequestToDataServer(LoginSession::DATA_SERVER_IDLE);
        }

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
        time_t now = time(NULL);
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
                mpSession = SessionTracker::GetInstance()->GetSessionDetails(AccountPacket.dwAccountID);
            }
            catch (std::runtime_error) {
                LOG_WARNING("Client tried to connect to data server before authenticating.");
                bError = true;
                break;
            }
            // Also verify that it's the same client that authenticated and that the session has not expired
            if (mpSession->GetClientIPAddress() != mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr) {
                LOG_WARNING("Account ID / IP address mismatch.");
                bError = true;
                break;
            }
            if (mpSession->HasExpired()) {
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
            uint8_t bufNewKey[24];
            if (mpConnection->ReadAll(bufNewKey, sizeof(bufNewKey)) != sizeof(bufNewKey)) {
                LOG_WARNING("Client sent an incomplete key packet.");
                bError = true;
                break;
            }
            mpSession->SetKey(bufNewKey);
            // Update the key in the session tracker, add some time to the TTL if needed
            LOG_DEBUG1("Receving key from client.");
            mpSession->SetExpiryTimeRelative(30);
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
        LOG_DEBUG1("Sending character list to client.");
        // Seems that xiloader doesn't like us to send the list too quickly
        std::this_thread::sleep_for(std::chrono::seconds(1));
        SendCharacterList();
        // Signal the view server that the character list packet can be sent
        mpSession->SendRequestToViewServer(LoginSession::VIEW_SERVER_SEND_CHARACTER_LIST);
    }
    LOG_INFO("Client successfully connected with account ID: %d", mdwAccountID);
    mpConnection->Close();
    mpSession->SetDataServerFinished();
    if (mpSession->IsViewServerFinished()) {
        // Both servers have finished so mark the session as expired
        // so it gets cleaned up immediately.
        mpSession->SetExpiryTimeAbsolute(0);
    }
    mbRunning = false;
}

void DataHandler::SendCharacterList()
{
    LOG_DEBUG0("Called.");

    DATA_PACKET_CHARACTER_ENTRY CharList[16];
    // Load character list from DB into session
    mpSession->LoadCharacterList();
    uint8_t cNumCharsAllowed = min(mpSession->GetNumCharsAllowed(), 16);
    uint8_t cNumChars = min(mpSession->GetNumCharacters(), cNumCharsAllowed);
    const CHARACTER_ENTRY* pCurrentChar;

    for (uint8_t i = 0; i < cNumChars; i++) {
        pCurrentChar = mpSession->GetCharacter(i);
        CharList[cNumChars].dwContentID = pCurrentChar->dwCharacterID;
        CharList[cNumChars].dwCharacterID = pCurrentChar->dwCharacterID;
    }
    // The header is the packet type and number of chars, and it actually
    // overwrites the first two bytes, but since they're guaranteed to be
    // zero anyway, that's fine.
    reinterpret_cast<uint8_t*>(&CharList)[0] = static_cast<uint8_t>(S2C_PACKET_CHARACTER_LIST);
    reinterpret_cast<uint8_t*>(&CharList)[1] = cNumChars;
    LOG_DEBUG1("Sending character list.");
    if (mpConnection->WriteAll(reinterpret_cast<uint8_t*>(&CharList), sizeof(CharList)) <= 0) {
        LOG_ERROR("Connection error when sending character ID list.");
        throw std::runtime_error("Connection dropped.");
    }
    LOG_DEBUG1("Character list send.");
}
