/**
 *	@file LoginHandler.h
 *	Implementation of the login server protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_LOGINHANDLER_H
#define FFXI_LOGIN_LOGINHANDLER_H

#include "TCPConnection.h"
#include <thread>
#include <memory>

/**
 *  Login handler class, create an object for each connecting client.
 */
class LoginHandler
{
public:
    /**
     *  Create a new handler.
     *  @param connection TCP connection to assign to this handler
     */
    LoginHandler(TCPConnection& connection);

    /**
     *  Generally it's advisable to explicitly call Shutdown before
     *  destroying the object.
     */
    ~LoginHandler();

    /**
     *  Run the handler. Should generally not be called directly,
     *  use StartThread() to run the handler in a separate thread.
     */
    void Run();

    /**
     *  Start running the handler as a separate thread.
     */
    void StartThread();

    /**
     *  Returns whether the handler is currently running.
     *  @return True if currently running, false otherwise.
     */
    bool IsRunning() const;

    /**
     *  Shutdown the connection. Note that it will shut down by itself
     *  when the user disconnects or completes login. This should generally
     *  be called only when the server itself is shutting down.
     *  @bJoin Whether to join if running as thread
     */
    void Shutdown(bool bJoin = true);

    /**
     *  Static wrapper to Run, in order to allow it to run from std::thread
     *  @param thisobj Needs to point to an already initializaed instance of this class.
     */
    static void stRun(LoginHandler* thisobj);

private:
    /// Associated TCP connection
    TCPConnection mConnection;
    /// Are we currently running
    bool mbRunning;
    /// Shutdown flag
    bool mbShutdown;
    /// Associated thread object
    std::shared_ptr<std::thread> mpThreadObj;
};

#endif
