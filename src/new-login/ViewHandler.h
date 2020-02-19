/**
 *	@file ViewHandler.h
 *	Implementation of the view protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_VIEWHANDLER_H
#define FFXI_LOGIN_VIEWHANDLER_H

#include "ProtocolHandler.h"
#include "FFXIPacket.h"
#include "LoginSession.h"
#include <memory>

/**
 *  View handler class, create an object for each connecting client.
 *  This protocol goes between the server and the game client itself,
 *  rather than the bootloader. The protocol is mostly reverse-engineered
 *  so some data may be unexplained and/or hardcoded. Corrections and
 *  documentation are very welcome.
 */
class ViewHandler : public ProtocolHandler
{
public:
    /**
     *  Create a new handler.
     *  @param connection TCP connection to assign to this handler
     */
    ViewHandler(std::shared_ptr<TCPConnection> connection);

    /**
     *  Generally it's advisable to explicitly call Shutdown before
     *  destroying the object.
     */
    virtual ~ViewHandler();

    /**
     *  Run the handler. Should generally not be called directly,
     *  use StartThread() to run the handler in a separate thread.
     */
    void Run();

private:

#pragma pack(push ,1)
    /**
     *  Entry of a character in the character list packet
     */
    struct VIEW_CHAR_LIST_ENTRY {
        uint32_t dwCharacterID;     // 0-->3
        uint32_t dwContentID;       // 4-->7
        uint32_t dwUnknown1;        // 8-->11       (0x00000001)
        char szCharacterName[16];   // 12-->27
        char szWorldName[16];       // 28-->43
        uint8_t cRace;              // 44
        uint8_t cUnknown2;          // 45
        uint8_t cMainJob;           // 46
        char bufUnknown3[9];        // 47-->55
        uint8_t cFace;              // 56
        uint8_t cUnknown4;          // 57           (0x02)
        uint16_t wHead;             // 58-->59
        uint16_t wBody;             // 60-->61
        uint16_t wHands;            // 62-->63
        uint16_t wLegs;             // 64-->65
        uint16_t wFeet;             // 66-->67
        uint16_t wMain;             // 68-->69
        uint16_t wSub;              // 70-->71
        uint8_t cZone1;             // 72
        uint8_t cMainJobLevel;      // 73
        char bufUnknown5[4];        // 74-->77      (0x01, 0x00, 0x02, 0x00)
        uint16_t wZone2;            // 78-->79
        char bufUnknown6[60];       // 80-->139
    };

    /**
     *  Full character list packet
     */
    struct VIEW_PACKET_CHARACTER_LIST
    {
        uint32_t dwContentIds;  // 0-->3
        VIEW_CHAR_LIST_ENTRY CharList[16];
    };

    /**
     *  Features and expansion packet
     */
    struct VIEW_PACKET_EXPANSION_AND_FEATURES
    {
        uint32_t dwUnknown;
        uint32_t dwExpansions;
        uint32_t dwFeatures;
    };

#pragma pack(pop)

    /**
     *  Check the client version and send back a list of features
     *  @param pRequestPacket The payload of the request packet
     */
    void CheckVersionAndSendFeatures(uint8_t* pRequestPacket);

    /**
     *  Send the accout character list to the client.
     */
    void SendCharacterList();

    /// FFXI Packet parser
    FFXIPacket mParser;
    /// Associated session
    std::shared_ptr<LoginSession> mpSession;
};

#endif