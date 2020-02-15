/**
 *	@file LoginServer.h
 *	TCP server routines
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_SERVER_H
#define FFXI_LOGIN_SERVER_H

#include <vector>
#include <memory>

#include <stdint.h>

#include "TCPConnection.h"
#include "LoginHandler.h"

/**
 *	Main login server class.
 */
class LoginServer
{
public:

	/**
	 *	Create new instance, does some OS level initialization.
	 */
	LoginServer();

	/**
	 *	Destructor.
	 */
	~LoginServer();

	/**
	 *	Add a new listening port.
	 *	@param wPortNum Port number to listen on
	 *	@param szIpAddress IP address to listen all (defaults to all interfaces)
	 *	@param bSecure Set to true for SSL interfaces (if supported)
	 *	@note This actually starts listening on the port
	 */
	void AddBind(uint16_t wPortNum, const char* szIpAddress = NULL, bool bSecure = false);

	/**
	 *	Run the server and serve connections until Shutdown() is called
	 *	from a different thread.
	 */
	void Run();

    /**
     *  Returns whether the handler is currently running.
     *  @return True if currently running, false otherwise.
     */
    bool IsRunning() const;

    /**
     *  Start the server in a separate thread. You should generally call this
     *  instead of run.
     */
    void StartThread();

	/**
	 *	Shut down the server and close all connections and listening sockets.
	 */
	void Shutdown(bool bJoin = true);

private:
	/// Current listening sockets
	std::vector<BoundSocket> mvecListeningSockets;
	/// Currently working handlers
	std::vector<std::shared_ptr<LoginHandler>> mvecWorkingHandlers;
	/// Shutdown flag
	bool mbShutdown;
	/// Whether server is currently running
	bool mbRunning;
    /// Associated thread object
    std::shared_ptr<std::thread> mpThreadObj;

    /**
     *  Static wrapper to Run, in order to allow it to run from std::thread
     *  @param thisobj Needs to point to an already initializaed instance of this class.
     */
    static void stRun(LoginServer* thisobj);
};

#endif
