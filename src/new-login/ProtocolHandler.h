/**
 *	@file ProtocolHandler.h
 *	Interface to various protocols implemented by the login server.
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_PROTOCOLHANDLER_H
#define FFXI_LOGIN_PROTOCOLHANDLER_H

#include "TCPConnection.h"
#include <thread>
#include <memory>

/**
 *  Abstrace class, derive a class from this for each protocol
 *  implemented by the login server.
 */
class ProtocolHandler
{
public:
    /**
     *  Create a new handler.
     *  @param connection TCP connection to assign to this handler
     */
    ProtocolHandler(std::shared_ptr<TCPConnection> connection);

    /**
     *  Generally it's advisable to explicitly call Shutdown before
     *  destroying the object.
     */
    virtual ~ProtocolHandler();

    /**
     *  Run the handler. Should generally not be called directly,
     *  use StartThread() to run the handler in a separate thread.
     */
    virtual void Run() = 0;

    /**
     *  Start running the handler as a separate thread.
     */
    virtual void StartThread();

    /**
     *  Returns whether the handler is currently running.
     *  @return True if currently running, false otherwise.
     */
    virtual bool IsRunning() const;

    /**
     *  Get the client TCP/IP details
     *  @return BoundSocket struct with the client details.
     */
    virtual const BoundSocket& GetClientDetails() const;

    /**
     *  Shutdown the connection. Note that it will shut down by itself
     *  when the user disconnects or completes login. This should generally
     *  be called only when the server itself is shutting down.
     *  @bJoin Whether to join if running as thread
     */
    virtual void Shutdown(bool bJoin = true);

protected:
    /// Associated TCP connection
    std::shared_ptr<TCPConnection> mpConnection;
    /// Are we currently running
    bool mbRunning;
    /// Shutdown flag
    bool mbShutdown;
    /// Associated thread object
    std::shared_ptr<std::thread> mpThreadObj;

    /**
     *  Static wrapper to Run, in order to allow it to run from std::thread
     *  @param thisobj Needs to point to an already initializaed instance of this class.
     */
    static void stRun(ProtocolHandler* thisobj);

};

#endif
