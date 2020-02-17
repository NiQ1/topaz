/**
 *	@file TCPConnection.h
 *	Low level TCP connection classes.
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_TCPCONNECTION_H
#define FFXI_LOGIN_TCPCONNECTION_H

#include <string>
#include <stdint.h>
#include <sys/types.h>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#define closesocket close
#defome SOCKET int
#endif

 /**
  *	Connection details. Plain simple data, stored in a vector to
  *	keep track of listening and connected sockets.
  */
struct BoundSocket
{
	/// Socket fd of the listening socket
	SOCKET iSock;
	/// Bind details
	struct sockaddr_in BindDetails;
	/// SSL flag
	bool bSecure;
};

class TCPConnection
{
public:

	/**
	 *	Constructor.
	 *	@param ConnectionDetails BoundSocket struct with connection socket and metadata
	 */
	TCPConnection(BoundSocket& ConnectionDetails);

	/**
	 *	Destructor will auto-close connections.
	 */
	virtual ~TCPConnection();

	/**
	 *	Read up to cbBytes bytes from the connection. Will generally block until
	 *	a packet has arrived.
	 *	@param bufReceived Buffer to receive the data into
	 *	@param cbBuffer Size of the buffer in bytes
	 *	@return Number of bytes received, 0 if connection closed, -1 on error
	 */
	virtual int32_t Read(uint8_t* bufReceived, int32_t cbBuffer);

	/**
	 *	Read exactly cbBytes bytes from the connection. Will call read repeadedly
	 *	until the requested number of bytes have been read.
	 *	@param bufReceived Buffer to receive the data into
	 *	@param cbMinRead Minumum number of bytes to read
	 *	@param cbMaxRead Optional maximum number of bytes to read (if not specified will read exactly cbMinRead)
	 *	@return Number of bytes received, 0 if connection closed, -1 on error
	 */
	virtual int32_t ReadAll(uint8_t* bufReceived, int32_t cbMinRead, int32_t cbMaxRead = 0);

	/**
	 *	Send data to the connection.
	 *	@param bufSend The buffer to send
	 *	@param cbData The size of the data
	 *	@return Number of bytes send or -1 on error.
	 */
	virtual int32_t Write(const uint8_t* bufSend, int32_t cbData);

	/**
	 *	Send data to the connection. Will send the entire buffer, may block
	 *	until all data has been sent.
	 *	@param bufSend The buffer to send
	 *	@param cbData The size of the data
	 *	@return Number of bytes send or -1 on error.
	 */
	virtual int32_t WriteAll(const uint8_t* bufSend, int32_t cbData);

	/**
	 *	Close the connection. Will interrupt any pending reads / writes.
	 */
	virtual void Close();

    /**
     *  Gets the bound socket details associated with this connection.
     *  @return BoundSocket struct associated with the connection.
     */
    const BoundSocket& GetConnectionDetails() const;

protected:
	/// Connection socket and details
	BoundSocket mConnectionDetails;

private:
	/// Whether the connection has already been closed
	bool mbClosed;
};

#endif
