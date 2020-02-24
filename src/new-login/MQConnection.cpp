/**
 *	@file MQConnection.cpp
 *	Handles connections to world MQ servers
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "MQConnection.h"
#include "Debugging.h"
#include "GlobalConfig.h"
#include "TCPConnection.h"
#include <stdexcept>
#include <chrono>

#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>

#define LOCK_MQCONNECTION std::lock_guard<std::recursive_mutex> l_mqconnection(*GetMutex())

MQConnection::MQConnection(uint32_t dwWorldId,
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
    size_t cbClientKey)
{
    LOG_DEBUG0("Called.");
    madwSendersWaiting = 0;
    GlobalConfigPtr Config = GlobalConfig::GetInstance();

    mConnection = amqp_new_connection();
    if (mConnection == NULL) {
        LOG_ERROR("AMQP initialization failed.");
        throw std::runtime_error("AMQP init error.");
    }
    mSocket = NULL;
    if (bUseSSL) {
        mSocket = amqp_ssl_socket_new(mConnection);
    }
    else {
        mSocket = amqp_tcp_socket_new(mConnection);
    }
    if (mSocket == NULL) {
        LOG_ERROR("AMQP socket initialization failed.");
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("AMQP socket init error.");
    }
    if (amqp_socket_open(mSocket, strMqServer.c_str(), wMqPort) != AMQP_STATUS_OK) {
        LOG_ERROR("Could not connect to MQ server.");
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ connection error.");
    }
    if (amqp_login(mConnection,
        strVHost.c_str(),
        AMQP_DEFAULT_MAX_CHANNELS,
        AMQP_DEFAULT_FRAME_SIZE,
        60,
        AMQP_SASL_METHOD_PLAIN,
        strUsername.c_str(),
        strPassword.c_str()).reply_type != AMQP_RESPONSE_NORMAL) {
        LOG_ERROR("Authentication to MQ server failed.");
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ login error.");
    }
    amqp_channel_open(mConnection, 1);
    if (amqp_get_rpc_reply(mConnection).reply_type != AMQP_RESPONSE_NORMAL) {
        LOG_ERROR("Opening new channel failed.");
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ channel open error.");
    }
    amqp_queue_declare(mConnection, 1, amqp_cstring_bytes(strQueueName.c_str()), 0, 0, 0, 1, amqp_empty_table);
    if (amqp_get_rpc_reply(mConnection).reply_type != AMQP_RESPONSE_NORMAL) {
        LOG_ERROR("Declaration of queue failed.");
        amqp_channel_close(mConnection, 1, AMQP_INTERNAL_ERROR);
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ queue declare error.");
    }
    amqp_queue_bind(mConnection,
        1,
        amqp_cstring_bytes(strQueueName.c_str()),
        amqp_cstring_bytes(strExchange.c_str()),
        amqp_cstring_bytes(strRouteKey.c_str()),
        amqp_empty_table);
    if (amqp_get_rpc_reply(mConnection).reply_type != AMQP_RESPONSE_NORMAL) {
        LOG_ERROR("Failed to bind queue to exchange.");
        amqp_channel_close(mConnection, 1, AMQP_INTERNAL_ERROR);
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ queue bind error.");
    }
    amqp_basic_consume(mConnection, 1, amqp_cstring_bytes(strQueueName.c_str()), amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    if (amqp_get_rpc_reply(mConnection).reply_type != AMQP_RESPONSE_NORMAL) {
        LOG_ERROR("Unable to start consumer.");
        amqp_queue_unbind(mConnection, 1, amqp_cstring_bytes(strQueueName.c_str()), amqp_cstring_bytes(strExchange.c_str()), amqp_empty_bytes, amqp_empty_table);
        amqp_channel_close(mConnection, 1, AMQP_INTERNAL_ERROR);
        amqp_destroy_connection(mConnection);
        throw std::runtime_error("MQ constume error.");
    }
    mstrExchange = strExchange;
    mstrQueueName = strQueueName;
    mstrRouteKey = strRouteKey;
}

MQConnection::~MQConnection()
{
    amqp_queue_unbind(mConnection, 1, amqp_cstring_bytes(mstrQueueName.c_str()), amqp_cstring_bytes(mstrExchange.c_str()), amqp_empty_bytes, amqp_empty_table);
    amqp_channel_close(mConnection, 1, AMQP_INTERNAL_ERROR);
    amqp_destroy_connection(mConnection);
}

void MQConnection::Run()
{
    amqp_rpc_reply_t Response;
    amqp_envelope_t Envelope;
    amqp_frame_t Frame;
    amqp_message_t Message;
    struct timeval tv = { 0, 1000 };

    LOG_DEBUG0("Called.");
    amqp_maybe_release_buffers(mConnection);

    while (mbShutdown == false) {
        if (madwSendersWaiting != 0) {
            // There are senders waiting to send data so give them some time to do their business
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LOCK_MQCONNECTION;

        Response = amqp_consume_message(mConnection, &Envelope, &tv, 0);
        if (Response.reply_type == AMQP_RESPONSE_NORMAL) {LOG_DEBUG1("Received message.");
            HandleRequest(Envelope.message.body);
        }
        else if (Response.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
            if (Response.library_error == AMQP_STATUS_TIMEOUT) {
                // No message
                continue;
            }
            else if (Response.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
                // Metadata packet
                LOG_DEBUG1("Received metadata packet.");
                if (amqp_simple_wait_frame(mConnection, &Frame) != AMQP_STATUS_OK) {
                    LOG_ERROR("Wait for frame failed.");
                    throw std::runtime_error("Wait for frame failed.");
                }
                switch (Frame.payload.method.id) {
                case AMQP_BASIC_ACK_METHOD:
                    // Simple ACK packet. Currently we just ignore these.
                    continue;
                case AMQP_BASIC_RETURN_METHOD:
                    // A message we've published with the mandatory flag up was not received
                    // by anyone. We may want to do something about it here if we ever have
                    // a reason to set the mandatory flag. For now just read the message and
                    // ignore it.
                    Response = amqp_read_message(mConnection, Frame.channel, &Message, 0);
                    if (Response.reply_type != AMQP_RESPONSE_NORMAL) {
                        LOG_ERROR("Error occured when attempting to read return method message.");
                        throw std::runtime_error("Return method read error.");
                    }
                    amqp_destroy_message(&Message);
                    break;
                case AMQP_CHANNEL_CLOSE_METHOD:
                    // This shouldn't generally happen. It may be possible to recover by
                    // re-creating the channel and re-declaring the queue but the probability
                    // is so low that it's not worthwhile.
                    LOG_ERROR("Channel closed by MQ server.");
                    throw std::runtime_error("Unexcpected MQ channel close.");
                case AMQP_CONNECTION_CLOSE_METHOD:
                    LOG_ERROR("Connection closed by MQ server.");
                    throw std::runtime_error("Unexpected MQ connection close.");
                }
                LOG_ERROR("Unknown library error code.");
                throw std::runtime_error("Unknown library error code.");
            }
            else {
                LOG_ERROR("Unknown library staus code.");
                throw std::runtime_error("Unknown library status code.");
            }
        }
        else {
            LOG_ERROR("Unknown reply received by consumer.");
            throw std::runtime_error("Unknown consumer reply.");
        }

        amqp_maybe_release_buffers(mConnection);
    }
}

void MQConnection::Send(uint8_t* bufData, uint32_t cbData)
{
    amqp_basic_properties_t Properties = { 0 };
    Properties._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
    Properties.content_type = amqp_cstring_bytes("application/octet-stream");
    amqp_bytes_t Message;
    Message.len = cbData;
    Message.bytes = bufData;
    if (amqp_basic_publish(mConnection,
        1,
        amqp_cstring_bytes(mstrExchange.c_str()),
        amqp_cstring_bytes(mstrRouteKey.c_str()),
        0,
        0,
        &Properties,
        Message) != AMQP_STATUS_OK) {
        LOG_ERROR("Failed to publish message.");
        throw std::runtime_error("Publish failed.");
    }
}

std::recursive_mutex* MQConnection::GetMutex()
{
    LOG_DEBUG0("Called.");
    return &mMutex;
}
