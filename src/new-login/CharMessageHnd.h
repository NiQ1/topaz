/**
 *	@file CharMessageHnd.h
 *	Message handler for character creation / login
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_CHARMESSAGEHND_H
#define FFXI_LOGIN_CHARMESSAGEHND_H

#include "MQHandler.h"
#include "MQConnection.h"

/**
 *  Character message handler. Supports creation, login and update.
 */
class CharMessageHnd : public MQHandler
{
public:

    /**
     *  Default constructor, doesn't do much.
     */
    CharMessageHnd();

    /**
     *  Destructor
     */
    virtual ~CharMessageHnd();

    /**
     *  Handle a single character MQ request
     *  @param Request Request bytes to handle
     *  @param pOrigin MQ connection from which the message originated
     *  @return true if the message has been processed, false to proceed to next handler
     */
    virtual bool HandleRequest(amqp_bytes_t Request, MQConnection* pOrigin);

#pragma pack(push, 1)

    struct CHAR_MQ_MESSAGE_HEADER
    {
        MQConnection::MQ_MESSAGE_TYPES eType;
        uint32_t dwCharacterID;
        uint32_t dwAccountID;
    };

    /**
     *  Full details of a single character
     */
    struct CHARACTER_ENTRY
    {
        uint32_t dwContentID;
        bool bEnabled;
        uint32_t dwCharacterID;
        char szCharName[16];
        uint8_t cWorldID;
        uint8_t cMainJob;
        uint8_t cMainJobLevel;
        uint16_t wZone;
        uint8_t cRace;
        uint8_t cFace;
        uint8_t cHair;
        uint8_t cSize;
        uint8_t cNation;
        // Whatever the char was wearing when last logged-out
        uint16_t wHead;
        uint16_t wBody;
        uint16_t wHands;
        uint16_t wLegs;
        uint16_t wFeet;
        // Equipped weapons, not jobs
        uint16_t wMain;
        uint16_t wSub;
    };

    /**
     *  Message that is sent from the login server to the world server when
     *  a user attempts to log-in.
     */
    struct MESSAGE_LOGIN_REQUEST
    {
        CHAR_MQ_MESSAGE_HEADER Header;
        uint8_t bufInitialKey[16];
        uint32_t dwIPAddress;
        // Map server may decide to allow/disallow content based on these
        uint32_t dwExpansions;
        uint32_t dwFeatures;
    };

    struct MESSAGE_LOGIN_RESPONSE
    {
        CHAR_MQ_MESSAGE_HEADER Header;
        // 0 for success or error code
        uint32_t dwResponseCode;
        // IP + port for the zone the character will appear in
        uint32_t dwZoneIP;
        uint16_t wZonePort;
        // IP + port of the global search server
        uint32_t dwSearchIP;
        uint16_t wSearchPort;
    };

#pragma pack(pop)

    /**
     *  Update character information in DB
     *  @param pNewDetails New details to set.
     *  @note Character ID is read from the struct, if it does not exist, it will be created.
     */
    static void UpdateCharacter(CHARACTER_ENTRY* pNewDetails);

    /**
     *  Get character details from the DB
     *  @param dwContentID Unique character content ID
     *  @return Character details struct
     */
    static CHARACTER_ENTRY QueryCharacter(uint32_t dwContentID);

    /**
     *  Get character details from the DB
     *  @param dwCharacterID Character ID to query
     *  @param cWorldID World ID for the character (character IDs are only unique within the world)
     *  @return Character details struct
     */
    static CHARACTER_ENTRY QueryCharacter(uint32_t dwCharacterID, uint8_t cWorldID);

};

#endif
