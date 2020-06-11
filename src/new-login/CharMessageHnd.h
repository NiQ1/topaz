/**
 *	@file CharMessageHnd.h
 *	Message handler for character creation / login
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_CHARMESSAGEHND_H
#define FFXI_LOGIN_CHARMESSAGEHND_H

#include "new-common/MQHandler.h"
#include "new-common/MQConnection.h"
#include "new-common/CommonMessages.h"

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
