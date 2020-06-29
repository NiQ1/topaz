/**
 *	@file FFXILoginPacket.cpp
 *	Implements the FFXI login packet header (for view server)
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "FFXILoginPacket.h"
#include "new-common/Debugging.h"
#include <openssl/md5.h>
#include <stdexcept>

// Do not allocate more than this size per packet
#define MAX_PACKET_SIZE_ALLOWED 1048576

FFXILoginPacket::FFXILoginPacket(std::shared_ptr<TCPConnection> Connection) : mpConnection(Connection)
{
    LOG_DEBUG0("Called.");
    mbufPacketMagic[0] = 'I';
    mbufPacketMagic[1] = 'X';
    mbufPacketMagic[2] = 'F';
    mbufPacketMagic[3] = 'F';
}

FFXILoginPacket::~FFXILoginPacket()
{
    // Currently empty, intentionally not closing the connection because
    // ProtocolHandler does it for us.
    LOG_DEBUG0("Called.");
}

std::shared_ptr<uint8_t> FFXILoginPacket::ReceivePacket()
{
    LOG_DEBUG0("Called.");
    // Get header first because it contains the length of the packet
    FFXI_PACKET_HEADER Header;
    if (mpConnection->ReadAll(reinterpret_cast<uint8_t*>(&Header), sizeof(FFXI_PACKET_HEADER)) <= 0) {
        LOG_WARNING("Client dropped connection while reading header.");
        throw std::runtime_error("Client dropped connection.");
    }
    if (memcmp(Header.bufMagic, mbufPacketMagic, sizeof(mbufPacketMagic)) != 0) {
        LOG_WARNING("Data is not a valid FFXI packet.");
        throw std::runtime_error("Garbage data received.");
    }
    if (Header.dwPacketSize + sizeof(Header) > MAX_PACKET_SIZE_ALLOWED) {
        // This will only happen if someone intentionally sets the size to
        // a huge number to attempt to crash the server.
        LOG_WARNING("Possible crash attempt - Packet size is too big.");
        throw std::runtime_error("Packet too big.");
    }
    uint8_t* PacketData = new uint8_t[Header.dwPacketSize + sizeof(FFXI_PACKET_HEADER)];
    memcpy(PacketData, &Header, sizeof(Header));
    // Read the rest of the packet
    if (mpConnection->ReadAll(PacketData + sizeof(Header), Header.dwPacketSize - sizeof(FFXI_PACKET_HEADER)) <= 0) {
        LOG_WARNING("Client dropped connection while reading data.");
        delete PacketData;
        throw std::runtime_error("Client dropped connection mid-packet.");
    }
    // Verify packet integrity
    // Disabled - Seems that newer client just send all zeros (but since we still may want to support
    // older clients then we will calculate MD5 for packets that we send).
    // memset(reinterpret_cast<FFXI_PACKET_HEADER*>(PacketData)->bufMD5, 0, sizeof(reinterpret_cast<FFXI_PACKET_HEADER*>(PacketData)->bufMD5));
    // uint8_t bufMD5[16];
    // MD5(PacketData, Header.dwPacketSize, bufMD5);
    // if (memcmp(bufMD5, Header.bufMD5, sizeof(bufMD5)) != 0) {
    //     LOG_WARNING("Packet MD5 mismatch.");
    //     delete PacketData;
    //     throw std::runtime_error("Packet MD5 mismatch.");
    // }
    //memcpy(reinterpret_cast<FFXI_PACKET_HEADER*>(PacketData)->bufMD5, Header.bufMD5, sizeof(reinterpret_cast<FFXI_PACKET_HEADER*>(PacketData)->bufMD5));
    LOG_DEBUG0("Received packet, %d bytes.", Header.dwPacketSize);
    return std::shared_ptr<uint8_t>(PacketData);
}

void FFXILoginPacket::SendPacket(uint8_t* pPacket)
{
    LOG_DEBUG0("Called.");
    if (memcmp(reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket)->bufMagic, mbufPacketMagic, sizeof(mbufPacketMagic)) != 0) {
        LOG_ERROR("Attempted to send data which is not an FFXI packet.");
        throw std::runtime_error("Not a valid FFXI packet.");
    }
    LOG_DEBUG0("Sending %d bytes long packet.", reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket)->dwPacketSize);
    if (mpConnection->WriteAll(pPacket, reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket)->dwPacketSize) != reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket)->dwPacketSize) {
        LOG_WARNING("Connection dropped while sending packet.");
        throw std::runtime_error("Connection dropped.");
    }
}

void FFXILoginPacket::SendPacket(FFXI_PACKET_TYPES eType, uint8_t* pData, uint32_t cbData)
{
    LOG_DEBUG0("Called.");
    if (cbData + sizeof(FFXI_PACKET_HEADER) > MAX_PACKET_SIZE_ALLOWED) {
        LOG_ERROR("Packet to send is too big.");
        throw std::runtime_error("Packet to send is too big.");
    }
    LOG_DEBUG0("Building packet.");
    // Allocate buffer for the packet including the header
    uint8_t* pPacket = new uint8_t[cbData + sizeof(FFXI_PACKET_HEADER)];
    FFXI_PACKET_HEADER* pHeader = reinterpret_cast<FFXI_PACKET_HEADER*>(pPacket);
    pHeader->dwPacketSize = cbData + sizeof(FFXI_PACKET_HEADER);
    memcpy(pHeader->bufMagic, mbufPacketMagic, sizeof(pHeader->bufMagic));
    pHeader->dwPacketType = static_cast<uint32_t>(eType);
    memcpy(pPacket + sizeof(FFXI_PACKET_HEADER), pData, cbData);
    memset(pHeader->bufMD5, 0, sizeof(pHeader->bufMD5));
    MD5(pPacket, cbData + sizeof(FFXI_PACKET_HEADER), pHeader->bufMD5);
    try {
        SendPacket(pPacket);
    }
    catch (...) {
        delete pPacket;
        throw;
    }
    delete pPacket;
}

void FFXILoginPacket::SendError(FFXI_ERROR_CODES ErrorCode)
{
    LOG_WARNING("Sending error %d to the client.", ErrorCode);
    FFXI_ERROR_PACKET ErrorPacket;
    ErrorPacket.dwErrorCode = static_cast<uint32_t>(ErrorCode);
    ErrorPacket.dwZero = 0;
    SendPacket(FFXI_TYPE_ERROR, reinterpret_cast<uint8_t*>(&ErrorPacket), sizeof(ErrorPacket));
}

void FFXILoginPacket::SendDone()
{
    LOG_DEBUG1("Sending done packet.");
    // Body is 4 bytes, meaning is unknown
    uint32_t dwBody = 0;
    SendPacket(FFXI_TYPE_DONE, reinterpret_cast<uint8_t*>(&dwBody), sizeof(dwBody));
}
