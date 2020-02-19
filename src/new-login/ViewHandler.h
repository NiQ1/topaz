/**
 *	@file ViewHandler.h
 *	Implementation of the view protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_VIEWHANDLER_H
#define FFXI_LOGIN_VIEWHANDLER_H

#include "ProtocolHandler.h"
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

};

#endif