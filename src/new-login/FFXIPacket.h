/**
 *	@file FFXIPacket.h
 *	Implements the FFXI login packet header (for view server)
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_FFXI_PACKET_H
#define FFXI_LOGIN_FFXI_PACKET_H

#include "TCPConnection.h"
#include <stdint.h>
#include <memory>

/**
 *  This allows sending and receiving FFXI login packets. These packets
 *  are used by the view server, which directly communicates with the
 *  game client (rather than the bootloader) so it needs to speak the
 *  FFXI protocol.
 */
class FFXIPacket {
public:

    /**
     *  Initialize a new object.
     *  @param Connection underlying TCP connection (login server uses TCP)
     */
    FFXIPacket(std::shared_ptr<TCPConnection> Connection);

    /**
     *  Destructor.
     *  Does not close the TCP connection.
     */
    ~FFXIPacket();

#pragma pack(push, 1)
    /**
     *  FFXI Packet header and data
     */
    struct FFXI_PACKET_HEADER
    {
        // Length of the packet (including the header)
        uint32_t dwPacketSize;
        // Magic ("XIFF")
        uint8_t bufMagic[4];
        // Packet type (see enum)
        uint32_t dwPacketType;
        // Packet MD5 hash (on everything, including header)
        uint8_t bufMD5[16];
    };
#pragma pack(pop)

    /**
     *  Known packet types. There may be other packet types that are
     *  currently unknown but these are all that are documented.
     */
    enum FFXI_PACKET_TYPES
    {
        // Your business with the server completes successfully (server will then disconnect)
        FFXI_TYPE_DONE = 0x03,
        // An error has occured (error code attached)
        FFXI_TYPE_ERROR = 0x04,
        // Server sends the expansions and features list associated with the account
        FFXI_TYPE_FEATURES_LIST = 0x05,
        // Request to log-in with an existing character
        FFXI_TYPE_LOGIN_REQUEST = 0x07,
        // Server provides details on the map server when logging in
        FFXI_TYPE_LOGIN_RESPONSE = 0x0B,
        // Client requests to delete a character
        FFXI_TYPE_DELETE_CHARACTER = 0x14,
        // Client requests the list of characters associated with the account
        FFXI_TYPE_GET_CHARACTER_LIST = 0x1F,
        // Server sends the account character list
        FFXI_TYPE_CHARACTER_LIST = 0x20,
        // User just approved the final confirmation of character creation
        FFXI_TYPE_CREATE_CHAR_CONFIRM = 0x21,
        // Client requests to create a new character
        FFXI_TYPE_CREATE_CHARACTER = 0x22,
        // Server sends the world list
        FFXI_TYPE_WORLD_LIST = 0x23,
        // Client requests the world list
        FFXI_TYPE_GET_WORLD_LIST = 0x24,
        // Client reports its version and requests the account expansion and features available
        FFXI_TYPE_GET_FEATURES = 0x26
    };

    /**
     *  Error codes sent in FFXI_TYPE_ERROR messages
     */
    enum FFXI_ERROR_CODES
    {
        // Unable to connect to world server
        FFXI_ERROR_MAP_CONNECT_FAILED = 305,
        // Character name already taken
        FFXI_ERROR_NAME_ALREADY_TAKEN = 313,
        // Character creation not allowed
        FFXI_ERROR_CREATE_DENIED = 314,
        // Log-in not allowed (maintenance mode)
        FFXI_ERROR_LOGIN_DENIED = 321,
        // The server does not support this client version
        FFXI_ERROR_VERSION_MISMATCH = 331
    };

    /**
     *  Receive a packet from the network.
     *  @return pointer to the received data, including header. The packet data follows the header.
     */
    std::shared_ptr<uint8_t> ReceivePacket();

    /**
     *  Send a raw packet
     *  @param pPacket Packet to send, including the header. The packet data should follow the header
     */
    void SendPacket(uint8_t* pPacket);

    /**
     *  Construct and sent a packet
     *  @param eType Packet type (see enum)
     *  @param pData Data to send (without header)
     *  @param cbData Size of the data (without header) in bytes
     */
    void SendPacket(FFXI_PACKET_TYPES eType, uint8_t* pData, uint32_t cbData);

    /**
     *  Sends an error packet to the client
     *  @param ErrorCode Error code to send (from FFXI_ERROR_CODES enum)
     */
    void SendError(FFXI_ERROR_CODES ErrorCode);

#pragma pack(push, 1)
    /**
     *  Structure of an error packet payload
     */
    struct FFXI_ERROR_PACKET
    {
        uint32_t dwZero;
        uint32_t dwErrorCode;
    };
#pragma pack(pop)

private:

    /// Connected TCP socket
    std::shared_ptr<TCPConnection> mpConnection;
    /// Packet magic
    uint8_t mbufPacketMagic[4];

};

#endif
