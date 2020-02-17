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
    LoginHandler(std::shared_ptr<TCPConnection> connection);

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
     *  Get the client TCP/IP details
     *  @return BoundSocket struct with the client details.
     */
    const BoundSocket& GetClientDetails() const;

    /**
     *  Shutdown the connection. Note that it will shut down by itself
     *  when the user disconnects or completes login. This should generally
     *  be called only when the server itself is shutting down.
     *  @bJoin Whether to join if running as thread
     */
    void Shutdown(bool bJoin = true);

private:
    /// Associated TCP connection
    std::shared_ptr<TCPConnection> mpConnection;
    /// Are we currently running
    bool mbRunning;
    /// Shutdown flag
    bool mbShutdown;
    /// Associated thread object
    std::shared_ptr<std::thread> mpThreadObj;
    /// Number of failed request so far
    uint16_t mwFailedRequests;

    /**
     *  Static wrapper to Run, in order to allow it to run from std::thread
     *  @param thisobj Needs to point to an already initializaed instance of this class.
     */
    static void stRun(LoginHandler* thisobj);

#pragma pack(push, 1)
    /**
     *  Structure of a login packet as it is transferred on the wire.
     */
    struct LoginPacket
    {
        char szUserName[16];
        char szPassword[16];
        // See enum below
        uint8_t ucCommandType;
        // These are newly introduced values
        // Used when changing password
        char szNewPassword[16];
        char szEmail[50];
        // Pad with some zeros because some modified bootloaders send more data
        // Note: It's only one char so do not change it to LOGIN_COMMANDS type.
        // Use a static cast as needed.
        char szZero[157];
    };
#pragma pack(pop)

    enum LOGIN_COMMANDS
    {
        // Login to existing account
        LOGIN_COMMAND_LOGIN = 0x10,
        // Create a new account
        LOGIN_COMMAND_CREATE = 0x20,
        // Below here these are new commands, unique to login-new
        // Change password
        LOGIN_COMMAND_CHANGE_PASSWORD = 0x80,
    };

#pragma pack(push, 1)
    /**
     *  Structure of the response packet sent from the server back
     *  to the client
     */
    struct LoginResponsePacket
    {
        // Response code (see enum below)
        uint8_t ucResponseType;
        // User account ID (for successful logins)
        uint32_t dwAccountId;
        // Reason for authentication failure
        uint16_t wFailureReason;
        // Zero pad
        char szZero[9];
    };
#pragma pack(pop)

    enum LOGIN_RESPONSES
    {
        LOGIN_SUCCESSFUL = 0x01,
        LOGIN_FAILED = 0x02,
        CREATE_SUCCESSFUL = 0x03,
        CREATE_FAILED = 0x04,
        PWCHANGE_SUCCESSFUL = 0x05,
        PWCHANGE_FAILED = 0x06,
        // Misc error codes
        MALFORMED_PACKET = 0x20
    };

    /**
     *  Verify the structure of a received login packet.
     *  @param Packet the packet to verify
     *  @return true if the packet has a correct structure, false if malformed
     */
    bool VerifyPacket(LoginPacket& Packet);

    /**
     *  Verify that a given string is properly NULL terminated.
     *  @param pszString String to verify
     *  @param dwMaxSize Maximum size of the string
     *  @return true if verified, false if no NULL terminator was found.
     */
    bool VerifyNullTerminatedString(const char* pszString, size_t dwMaxSize);
};

#endif
