/**
 *	@file FFXISearchPacket.h
 *	Implements the FFXI search / AH packet header
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_WORLD_FFXI_SEARCH_PACKET_H
#define FFXI_WORLD_FFXI_SEARCH_PACKET_H

#include "new-common/FFXIPacket.h"
#include "new-common/TCPConnection.h"
#include <stdint.h>
#include <memory>

// Structure of a search packet -
// 32-bit packet length (little endian)
// 32-bit magic (IXFF)
// Encrypted portion (modified Blowfish) -
// -- 32-bit packet type
// -- variable packet payload
// -- 128-bit MD5 hash of the type+payload
// 32-bit portion of the encryption key, which change on every
//     packet sent (the rest of the 128-bit key is constant)

 /**
  *  This allows sending and receiving FFXI search packets. These
  *  are used for searching characters, auction house, level sync etc.
  */
class FFXISearchPacket : public FFXIPacket {
public:

    /**
     *  Initialize a new object.
     *  @param Connection underlying TCP connection (login server uses TCP)
     */
    FFXISearchPacket(std::shared_ptr<TCPConnection> Connection);

    /**
     *  Destructor.
     *  Does not close the TCP connection.
     */
    virtual ~FFXISearchPacket();

    /**
     *  Known packet types. There may be other packet types that are
     *  currently unknown but these are all that are documented.
     */
    enum FFXI_SEARCH_PACKET_TYPES
    {
        FFXI_SEARCH_TYPE_SEARCH_ALL = 0x00,
        FFXI_SEARCH_TYPE_GROUP_LIST = 0x02,
        FFXI_SEARCH_TYPE_SEARCH = 0x03,
        FFXI_SEARCH_TYPE_AH_HISTORY_SINGLE = 0x05,
        FFXI_SEARCH_TYPE_AH_HISTORY_STACK = 0x06,
        FFXI_SEARCH_TYPE_SEARCH_COMMENT = 0x08,
        FFXI_SEARCH_TYPE_AH_REQUEST_MORE = 0x10,
        FFXI_SEARCH_TYPE_AH_REQUEST = 0x15
    };

    /**
     *  Receive a packet from the network.
     *  @return pointer to the received data, including header. The packet data follows the header.
     */
    virtual std::shared_ptr<uint8_t> ReceivePacket();

    /**
     *  Send a raw packet
     *  @param pPacket Packet to send, including the header. The packet data should follow the header
     */
    virtual void SendPacket(uint8_t* pPacket);

private:
    /// Encryption key to be used for the next packet
    uint8_t mbufPacketKey[24];
};

#endif
