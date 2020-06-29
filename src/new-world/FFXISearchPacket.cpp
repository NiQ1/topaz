/**
 *	@file FFXISearchPacket.cpp
 *	Implements the FFXI search / AH packet header
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "FFXISearchPacket.h"
#include "new-common/Debugging.h"
#include "new-common/BlowfishMod.h"
#include <openssl/md5.h>
#include <stdexcept>

 // Do not allocate more than this size per packet
#define SEARCH_MAX_PACKET_SIZE_ALLOWED 1048576


// Initial key used by the search server. Rotates with each packet.
const uint8_t gbufSearchInitialKey[24] =
{
    0x30, 0x73, 0x3D, 0x6D,
    0x3C, 0x31, 0x49, 0x5A,
    0x32, 0x7A, 0x42, 0x43,
    0x63, 0x38, 0x7B, 0x7E,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

FFXISearchPacket::FFXISearchPacket(std::shared_ptr<TCPConnection> Connection) : FFXIPacket(Connection)
{
    LOG_DEBUG0("Called.");
    // The encryption key rotates on each packet but always starts with a fixed key
    memcpy(mbufPacketKey, gbufSearchInitialKey, sizeof(mbufPacketKey));
}

FFXISearchPacket::~FFXISearchPacket()
{
    // Currently empty, intentionally not closing the connection because
    // ProtocolHandler does it for us.
    LOG_DEBUG0("Called.");
}

std::shared_ptr<uint8_t> FFXISearchPacket::ReceivePacket()
{
    // Get raw data
    std::shared_ptr<uint8_t> packet = FFXIPacket::ReceivePacket();
    uint8_t* data = packet.get();
    const FFXI_PACKET_HEADER* header = reinterpret_cast<const FFXI_PACKET_HEADER*>(data);
    // The encryption key rotates with each packet. The last 4 bytes
    // of each packet are the 4 bytes that rotate. The rest of the
    // key remains constant.
    *reinterpret_cast<uint32_t*>(mbufPacketKey + 16) = *reinterpret_cast<const uint32_t*>(data + header->dwPacketSize - 4);
    // And decrypt
    BLOWFISH_MOD_KEY KeyTable;
    bfmod_init_table(&KeyTable, reinterpret_cast<char*>(mbufPacketKey), sizeof(mbufPacketKey));
    // Offset 8 because size and magic parts of the header are not encrypted
    // Minus 28 bytes for size (4 bytes), magic (4 bytes), MD5 (16 bytes) and rotating key part (4 bytes)
    bfmod_decrypt_inplace(&KeyTable, reinterpret_cast<char*>(data + 8), header->dwPacketSize - 28);
    // Finally, verify that decryption was successful by checking the MD5 hash of the
    // packet, which is at offset -20 from the end of the packet (right before the key).
    uint8_t bufMD5[16];
    MD5(data + 8, header->dwPacketSize - 28, bufMD5);
    if (memcmp(bufMD5, data + header->dwPacketType - 20, sizeof(bufMD5)) != 0) {
         LOG_WARNING("Packet decryption failed (MD5 mismatch).");
         throw std::runtime_error("Packet decryption failed.");
    }
}

void FFXISearchPacket::SendPacket(uint8_t* pPacket)
{
    // Copy the packet to a different buffer so we don't touch the source
    FFXI_PACKET_HEADER* pHeader = reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket);
    if (pHeader->dwPacketSize > SEARCH_MAX_PACKET_SIZE_ALLOWED) {
        LOG_ERROR("Packet to send is too big.");
        throw std::runtime_error("Packet too big.");
    }
    std::unique_ptr<uint8_t> pPacketCopy(new uint8_t[pHeader->dwPacketSize]);
    memcpy(pPacketCopy.get(), pPacket, pHeader->dwPacketSize);
    // Calculate the MD5 hash of the data and store to the appropriate location
    MD5(pPacketCopy.get() + 8, pHeader->dwPacketSize - 28, pPacketCopy.get() + pHeader->dwPacketSize - 20);
    // We don't really bother rotating the key ourselves
    *reinterpret_cast<uint32_t*>(pPacketCopy.get() + pHeader->dwPacketSize - 4) = *reinterpret_cast<uint32_t*>(mbufPacketKey + 16);
    // Encrypt everything
    BLOWFISH_MOD_KEY KeyTable;
    bfmod_init_table(&KeyTable, reinterpret_cast<char*>(mbufPacketKey), sizeof(mbufPacketKey));
    bfmod_encrypt_inplace(&KeyTable, reinterpret_cast<char*>(pPacketCopy.get() + 8), pHeader->dwPacketSize - 28);
    // Can send now
    FFXIPacket::SendPacket(pPacketCopy.get());
}
