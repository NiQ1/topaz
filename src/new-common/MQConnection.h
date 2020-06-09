/**
 *	@file MQConnection.h
 *	Handles connections to world MQ servers
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_MQCONNECTION_H
#define FFXI_LOGIN_MQCONNECTION_H

#include "Thread.h"
#include "MQHandler.h"
#include <amqp.h>
#include <mutex>
#include <atomic>
#include <vector>

/**
 *  Represents a single connection to a MQ server for a single world
 */
class MQConnection : public Thread
{
public:

    /**
     *  Initialize a connection to a world MQ server.
     *  @param dwWorldId World ID to associate the connection to
     *  @param strMqServer Server host name or IP address
     *  @param wMqPort Port of the MQ servr
     *  @param strUsername Username to login with
     *  @param strPassword Password to login with
     *  @param strVHost Virtual host to use
     *  @param bUseSSL Whether to secure the connection with SSL
     *  @param bVerifyPeer Whether to verify the server certificate
     *  @param bufCACert CA certificate (used if bVerfifyPeer is enabled)
     *  @param cbCACert Size of bufCACert in bytes
     *  @param bufClientCert Client certificate to present (optional)
     *  @param cbClientCert Size of bufClientCert in bytes
     *  @param bufClientKey Private key matching the client certificate
     *  @param cbClientKey Size of bufClientKey in bytes.
     */
    MQConnection(uint32_t dwWorldId,
        const std::string& strMqServer,
        uint16_t wMqPort,
        const std::string& strUsername,
        const std::string& strPassword,
        const std::string& strVHost,
        const std::string& strExchange,
        const std::string& strQueueName,
        const std::string& strRouteKey,
        bool bUseSSL,
        bool bVerifyPeer,
        const uint8_t* bufCACert,
        size_t cbCACert,
        const uint8_t* bufClientCert,
        size_t cbClientCert,
        const uint8_t* bufClientKey,
        size_t cbClientKey);

    /**
     *  Destructor. Disconnects from the MQ
     */
    ~MQConnection();

    /**
     *  Get the World ID associated with this connection
     *  @return World ID
     */
    uint32_t GetWorldId() const;

    /**
     *  Assign a new handler to the connection. When a message is received
     *  all handlers will be called until one function returns a true value,
     *  in which case the iteration is stopped.
     *  @param pNewHandler New handler to register
     */
    void AssignHandler(std::shared_ptr<MQHandler> pNewHandler);

    /**
     *  Gets the session mutex object. Lock this before doing any changes.
     *  @return MQ Connection mutex object.
     */
    std::recursive_mutex* GetMutex();

    /**
     *  Runs the MQ consumer thread.
     */
    void Run();

    /**
     *  Send a message to the MQ server.
     *  @param bufData Data buffer to send
     *  @param cbData Size of the data in bytes
     */
    void Send(const uint8_t* bufData, uint32_t cbData);

    /**
     *  Message type codes for messages going between login and map servers
     */
    enum MQ_MESSAGE_TYPES
    {
        // Login server requests map servers to update the data on characters
        // associated with a given account
        MQ_MESSAGE_GET_ACCOUNT_CHARS = 1,
        // Full update of character details
        MQ_MESSAGE_CHAR_UPDATE = 2,
        // Character is about to log-in
        MQ_MESSAGE_CHAR_LOGIN = 3,
        // Map server acknowleges character login
        MQ_MESSAGE_CHAR_LOGIN_ACK = 4,
        // Character changed zone notification
        MQ_MESSAGE_CHAR_ZONE = 5,
        // Character changed gear notification
        MQ_MESSAGE_CHAR_GEAR = 6,
        // New character being created
        MQ_MESSAGE_CHAR_CREATE = 7,
        // Map server acknowleges character creation
        MQ_MESSAGE_CHAR_CREATE_ACK = 5,
        // Character being deleted
        MQ_MESSAGE_CHAR_DELETE = 8,
        // Map server acknowledges character deletion
        MQ_MESSAGE_CHAR_DELETE_ACK = 9,
        // Request to reserve a char that is about to be created
        MQ_MESSAGE_CHAR_RESERVE = 10,
        // Map server acknowledges that a char is reserved
        MQ_MESSAGE_CHAR_RESERVE_ACK = 11,
        // Login server requests full sync on all chars (use with caution!)
        MQ_MESSAGE_LOGIN_FULL_SYNC = 12,
        // Universal system message (on all worlds)
        MQ_MESSAGE_UNIVERSAL_ANNOUNCEMENT = 13,
    };

private:

    /// World ID associated with this connection
    uint32_t mdwWorldId;
    /// Internal handles used by the AMQP library to identify the connection
    amqp_connection_state_t mConnection;
    amqp_socket_t* mSocket;
    // Queue name for this session
    std::string mstrQueueName;
    // Exchange for this session
    std::string mstrExchange;
    // Routing key for this session
    std::string mstrRouteKey;
    /// List of message handlers registered with this connection
    std::vector<std::shared_ptr<MQHandler>> mpHandlers;

    // Mutex for access sync
    std::recursive_mutex mMutex;
    // Counts the number of senders that want to send data
    std::atomic<uint32_t> madwSendersWaiting;
};

#endif
