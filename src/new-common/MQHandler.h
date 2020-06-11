/**
 *	@file MQHandler.h
 *	Abstract class that handles and parses incoming MQ messages
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_COMMON_MQHANDLER_H
#define FFXI_COMMON_MQHANDLER_H

#include <amqp.h>

class MQConnection;

/**
 *  Abstract MQ message handler. Derive a class to implement parsing and
 *  handling of messages.
 */
class MQHandler
{
public:

    /**
     *  Default constructor, doesn't do much.
     */
    MQHandler();

    /**
     *  Destructor
     */
    virtual ~MQHandler();

    /**
     *  Handle a single MQ request
     *  @param Request Request bytes to handle
     *  @param pOrigin MQ connection from which the message originated
     *  @return true if the message has been processed, false to proceed to next handler
     */
    virtual bool HandleRequest(amqp_bytes_t Request, MQConnection* pOrigin) = 0;
};

#endif
