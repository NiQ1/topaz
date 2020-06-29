/**
 *	@file CharCreateHnd.h
 *	MQ Message handler for character creation and deletion
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_WORLD_CHARCREATEHND
#define FFXI_WORLD_CHARCREATEHND

#include "new-common/MQHandler.h"
#include "new-common/MQConnection.h"
#include "new-common/CommonMessages.h"
#include <vector>

/**
 *  Character creation message handler. Supports creation and deletion.
 */
class CharCreateHnd : public MQHandler
{
public:
    /**
     *  Default constructor, doesn't do much.
     */
    CharCreateHnd();

    /**
     *  Destructor
     */
    virtual ~CharCreateHnd();

    /**
     *  Handle a single character creation MQ request
     *  @param Request Request bytes to handle
     *  @param pOrigin MQ connection from which the message originated
     *  @return true if the message has been processed, false to proceed to next handler
     */
    virtual bool HandleRequest(amqp_bytes_t Request, MQConnection* pOrigin);

    /**
     *  Reserve a character ID in the DB for a new character.
     *  @param dwAccountID Account ID of the character being created
     *  @param dwContentID Content ID associated with the character
     *  @param dwCharacterID Character ID to reserve
     */
    static void ReserveCharacter(uint32_t dwAccountID, uint32_t dwContentID, uint32_t dwCharacterID);

    /**
     *  Create a new character.
     *  @param dwCharacterID ID of the new character, which must have been reserved
     *         by a prior call to ReserveCharacter.
     *  @param pCharEntry Details of the new character being created.
     *  @return The assigned character ID
     */
    static uint32_t CreateCharacter(uint32_t dwCharacterID, const CHARACTER_ENTRY* pCharEntry);

    /**
     *  Delete a character.
     *  @param dwCharacterID ID of the character to delete
     */
    static void DeleteCharacter(uint32_t dwCharacterID);

private:
    /// Data used for reserving characters
    struct mstReservationDetails
    {
        uint32_t dwCharID;
        uint32_t dwContentID;
        uint32_t dwAccountID;
        time_t tmExpiry;
    };
    /// Character IDs reserved but not yet committed to DB
    static std::vector<mstReservationDetails> mvecReservedCharIDs;
};

#endif
