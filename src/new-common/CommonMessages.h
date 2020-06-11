/**
 *	@file CommonMessages.h
 *	Defintion of structures going in MQ messages
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_COMMON_COMMONMESSAGES
#define FFXI_COMMON_COMMONMESSAGES

 // Name of the login server queue
#define LOGIN_MQ_NAME "LOGIN_MQ"

#pragma pack(push, 1)

struct CHAR_MQ_MESSAGE_HEADER
{
    MQConnection::MQ_MESSAGE_TYPES eType;
    uint32_t dwContentID;
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
 *  Any generic response packet that contains only the header
 *  and a response code.
 */
struct MESSAGE_GENERIC_RESPONSE
{
    CHAR_MQ_MESSAGE_HEADER Header;
    uint32_t dwResponseCode;
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

/**
 *  Confirmation or rejection of MESSAGE_LOGIN_REQUEST. Send from the
 *  world server to the login server.
 */
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

/**
 *  Login server initiates a character creation. Sent to the world server
 *  to reserve a character ID and a character name.
 */
struct MESSAGE_CREATE_REQUEST
{
    CHAR_MQ_MESSAGE_HEADER Header;
    char szCharName[16];
};

/**
 *  Sent to confirm character creation and provide all character details.
 *  This is sent to the world server after the user selects the starting
 *  nation and confirms character creation.
 */
struct MESSAGE_CONFIRM_CREATE_REQUEST
{
    CHAR_MQ_MESSAGE_HEADER Header;
    CHARACTER_ENTRY Details;
};

/**
 *  Response sent from the world server to the login server after receiving
 *  a request to confirm a character creation. It contains the initial zone
 *  the character starts at.
 */
struct MESSAGE_CONFIRM_CREATE_RESPONSE
{
    CHAR_MQ_MESSAGE_HEADER Header;
    uint32_t dwResponseCode;
    uint16_t wZone;
};

#pragma pack(pop)

#endif
