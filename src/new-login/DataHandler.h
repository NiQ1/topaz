/**
 *	@file DataHandler.h
 *	Implementation of the data protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_DATAHANDLER_H
#define FFXI_LOGIN_DATAHANDLER_H

#include "ProtocolHandler.h"
#include "TCPConnection.h"
#include <stdint.h>

 /**
 *  Login handler class, create an object for each connecting client.
 */
class DataHandler : public ProtocolHandler
{
public:
    /**
     *  Create a new handler.
     *  @param connection TCP connection to assign to this handler
     */
    DataHandler(std::shared_ptr<TCPConnection> connection);

    /**
     *  Generally it's advisable to explicitly call Shutdown before
     *  destroying the object.
     */
    virtual ~DataHandler();

    /**
     *  Run the handler. Should generally not be called directly,
     *  use StartThread() to run the handler in a separate thread.
     */
    void Run();

private:
    /// Account ID of the connected user
    uint32_t mdwAccountID;

    /**
     *  Packet types that the server sends to the client
     */
    enum SERVER_TO_CLIENT_PACKET_TYPES
    {
        // Request that the client sends its account id
        S2C_PACKET_SEND_ACCOUNT_ID = 1,
        // Request that the client sends an initial encryption key
        S2C_PACKET_SEND_KEY = 2,
        // Provides the client with the character list associated with the account
        S2C_PACKET_CHARACTER_LIST = 3
    };

    /**
     *  Packet types that the client sends to the server
     */
    enum CLIENT_TO_SERVER_PACKET_TYPES
    {
        // Client sends its account ID
        C2S_PACKET_ACCOUNT_ID = 0xA1,
        // Client sends an initial encryption key
        C2S_PACKET_KEY = 0xA2
    };

#pragma pack(push, 1)
    /**
     *  Account ID packet sent by the client
     */
    struct ACCOUNT_ID_PACKET
    {
        uint32_t dwAccountID;
        uint32_t dwServerAddress;
    };
#pragma pack(pop)

};

#endif
