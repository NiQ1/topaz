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
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>

#define LOCK_MQCONNECTION std::lock_guard<std::recursive_mutex> l_mqconnection(*GetMutex())

// We need to mess around with this struct to load certificates directly from buffer
struct amqp_ssl_socket_t {
    const struct amqp_socket_class_t *klass;
    SSL_CTX *ctx;
    int sockfd;
    SSL *ssl;
    amqp_boolean_t verify_peer;
    amqp_boolean_t verify_hostname;
    int internal_error;
};


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
    size_t cbClientKey) : mdwWorldId(dwWorldId)
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
        // NOTE: Out certificates / private keys are stored as DB blobs. Since the AMQP library
        // does not support reading certificates and keys from memory we'll need to use the
        // underlying OpenSSL routines to do this. Ugly but needed.
        LOG_DEBUG1("Using SSL for MQ connection.");
        mSocket = amqp_ssl_socket_new(mConnection);
        amqp_boolean_t bVerify = (bVerifyPeer && (cbCACert > 0)) ? 1 : 0;
        amqp_ssl_socket_set_verify_peer(mSocket, bVerify);
        amqp_ssl_socket_set_verify_hostname(mSocket, bVerify);
        if (bVerify) {
            LOG_DEBUG1("Verify peer enabled, installing CA certificate.");
            BIO* pBIO = BIO_new(BIO_s_mem());
            if (!pBIO) {
                LOG_ERROR("BIO allocation failed.");
                throw std::runtime_error("BIO allocation failed.");
            }
            if (BIO_puts(pBIO, reinterpret_cast<const char*>(bufCACert)) != 1) {
                BIO_free(pBIO);
                LOG_ERROR("CA certificate read failed.");
                throw std::runtime_error("CA Cert read failed.");
            }
            X509* pCert = PEM_read_bio_X509(pBIO, NULL, 0, NULL);
            if (!pCert) {
                BIO_free(pBIO);
                LOG_ERROR("CA Cert X509 parse failed.");
                throw std::runtime_error("CA X509 parse failed.");
            }
            struct amqp_ssl_socket_t* pSSLSocket = (struct amqp_ssl_socket_t *)mSocket;
            X509_STORE* pSSLStore = SSL_CTX_get_cert_store(pSSLSocket->ctx);
            if (!pSSLStore) {
                BIO_free(pBIO);;
                LOG_ERROR("Failed to obtain SSL store.");
                throw std::runtime_error("Get SSL store failed.");
            }
            int result = X509_STORE_add_cert(pSSLStore, pCert);
            BIO_free(pBIO);
            if (result != 1) {
                LOG_ERROR("Unable to install CA certificate.");
                throw std::runtime_error("CA cert install failed.");
            }
            LOG_DEBUG1("CA certificate installed.");
        }
        if ((cbClientCert > 0) && (cbClientKey > 0)) {
            LOG_DEBUG1("Client certificate provided, installing.");
            struct amqp_ssl_socket_t* pSSLSocket = (struct amqp_ssl_socket_t *)mSocket;
            BIO* pBIO = BIO_new(BIO_s_mem());
            if (!pBIO) {
                LOG_ERROR("Client cert BIO allocation failed.");
                throw std::runtime_error("Client cert BIO allocation failed.");
            }
            if (BIO_puts(pBIO, reinterpret_cast<const char*>(bufClientCert)) != 1) {
                BIO_free(pBIO);
                LOG_ERROR("Client certificate read failed.");
                throw std::runtime_error("Client Cert read failed.");
            }
            STACK_OF(X509_INFO)* pInfo = PEM_X509_INFO_read_bio(pBIO, NULL, NULL, NULL);
            if (!pInfo) {
                BIO_free(pBIO);
                LOG_ERROR("CA Cert X509 parse failed.");
                throw std::runtime_error("CA X509 parse failed.");
            }
            int iNumCerts = sk_X509_INFO_num(pInfo);
            if (iNumCerts <= 0) {
                sk_X509_INFO_pop_free(pInfo, X509_INFO_free);
                BIO_free(pBIO);
                LOG_ERROR("Invalid number of client certificates");
                throw std::runtime_error("Client Cert number invalid.");
            }
            X509_INFO* pCurrentInfo = sk_X509_INFO_value(pInfo, 0);
            if (!pCurrentInfo->x509) {
                sk_X509_INFO_pop_free(pInfo, X509_INFO_free);
                BIO_free(pBIO);
                LOG_ERROR("Unable to parse client certificate");
                throw std::runtime_error("Client Cert X509 parse failed.");
            }
            if (SSL_CTX_use_certificate(pSSLSocket->ctx, pCurrentInfo->x509) != 1) {
                sk_X509_INFO_pop_free(pInfo, X509_INFO_free);
                BIO_free(pBIO);
                LOG_ERROR("Cannot use the given client certificate.");
                throw std::runtime_error("Client Cert use failed.");
            }
            SSL_CTX_clear_chain_certs(pSSLSocket->ctx);
            pCurrentInfo->x509 = NULL;
            for (int i = 1; i < iNumCerts; i++) {
                pCurrentInfo = sk_X509_INFO_value(pInfo, i);
                if (!pCurrentInfo->x509) {
                    LOG_WARNING("Invalid X509 certificate in chain, ignoring.");
                    continue;
                }
                if (SSL_CTX_add0_chain_cert(pSSLSocket->ctx, pCurrentInfo->x509) != 1) {
                    LOG_WARNING("Unable to add chain certificate, ignoring.");
                    continue;
                }
                pCurrentInfo->x509 = NULL;
            }
            sk_X509_INFO_pop_free(pInfo, X509_INFO_free);
            BIO_free(pBIO);
            // Load private key
            pBIO = BIO_new(BIO_s_mem());
            if (!pBIO) {
                LOG_ERROR("Client private key BIO allocation failed.");
                throw std::runtime_error("Private key BIO allocation failed.");
            }
            if (BIO_puts(pBIO, reinterpret_cast<const char*>(bufClientKey)) != 1) {
                BIO_free(pBIO);
                LOG_ERROR("Private key read failed.");
                throw std::runtime_error("Private key read failed.");
            }
            RSA* pKey = PEM_read_bio_RSAPrivateKey(pBIO, NULL, NULL, NULL);
            if (!pKey) {
                BIO_free(pBIO);
                LOG_ERROR("Private key parse failed.");
                throw std::runtime_error("Private key parse failed.");
            }
            if (SSL_CTX_use_RSAPrivateKey(pSSLSocket->ctx, pKey) != 1) {
                RSA_free(pKey);
                BIO_free(pBIO);
                LOG_ERROR("Private key use failed.");
                throw std::runtime_error("Private key use failed.");
            }
            RSA_free(pKey);
            BIO_free(pBIO);
            LOG_DEBUG1("Client certificate installed.");
        }
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
    if (!strExchange.empty()) {
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
    size_t dwNumHandlers = 0;
    size_t i = 0;

    LOG_DEBUG0("Called.");
    amqp_maybe_release_buffers(mConnection);
    LOG_DEBUG1("MQ consumer started.");

    while (mbShutdown == false) {
        if (madwSendersWaiting != 0) {
            // There are senders waiting to send data so give them some time to do their business
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LOCK_MQCONNECTION;

        Response = amqp_consume_message(mConnection, &Envelope, &tv, 0);
        if (Response.reply_type == AMQP_RESPONSE_NORMAL) {
            LOG_DEBUG1("Received message.");
            dwNumHandlers = mpHandlers.size();
            for (i = 0; i < dwNumHandlers; i++) {
                if (mpHandlers[i]->HandleRequest(Envelope.message.body, this)) {
                    break;
                }
            }
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
    LOG_DEBUG1("MQ consumer finished.");
}

void MQConnection::Send(const uint8_t* bufData, uint32_t cbData)
{
    LOG_DEBUG0("Called.");
    madwSendersWaiting++;
    LOCK_MQCONNECTION;
    amqp_basic_properties_t Properties = { 0 };
    Properties._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
    Properties.content_type = amqp_cstring_bytes("application/octet-stream");
    amqp_bytes_t Message;
    Message.len = cbData;
    // Unfortunately this is needed (we don't want to modify external libs)
    // amqp_basic_publish is not expected to touch the buffer anyway.
    Message.bytes = const_cast<uint8_t*>(bufData);
    if (amqp_basic_publish(mConnection,
        1,
        amqp_cstring_bytes(mstrExchange.c_str()),
        amqp_cstring_bytes(mstrRouteKey.c_str()),
        0,
        0,
        &Properties,
        Message) != AMQP_STATUS_OK) {
        LOG_ERROR("Failed to publish message.");
        madwSendersWaiting--;
        throw std::runtime_error("Publish failed.");
    }
    madwSendersWaiting--;
    LOG_DEBUG1("Published message.");
}

std::recursive_mutex* MQConnection::GetMutex()
{
    return &mMutex;
}

uint32_t MQConnection::GetWorldId() const
{
    return mdwWorldId;
}

void MQConnection::AssignHandler(std::shared_ptr<MQHandler> pNewHandler)
{
    mpHandlers.push_back(pNewHandler);
}
