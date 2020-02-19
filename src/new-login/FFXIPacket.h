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
        FFXI_TYPE_SUCCESS = 0x03,
        FFXI_TYPE_ERROR = 0x04
    };

    /**
     *  Error codes sent in FFXI_TYPE_ERROR messages
     */
    enum FFXI_ERROR_CODES
    {
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

private:

    /// Connected TCP socket
    std::shared_ptr<TCPConnection> mpConnection;
    /// Packet magic
    uint8_t mbufPacketMagic[4];

};

#endif
