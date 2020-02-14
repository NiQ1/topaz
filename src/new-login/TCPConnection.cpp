/**
 *	@file TCPConnection.cpp
 *	Low level TCP connection classes.
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "TCPConnection.h"
#include "Debuggnig.h"

#ifdef _WIN32
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define SHUT_RDWR SD_BOTH
#endif

TCPConnection::TCPConnection(BoundSocket& ConnectionDetails) : mConnectionDetails(ConnectionDetails), mbClosed(false)
{
	LOG_DEBUG0("Called.");
}

TCPConnection::~TCPConnection()
{
	LOG_DEBUG0("Called.");
	if (mbClosed == false) {
		Close();
	}
}

int32_t TCPConnection::Read(uint8_t* bufReceived, int32_t cbBuffer)
{
	// Bytes read
	int32_t cbRead = 0;

	LOG_DEBUG0("Called.");
	if (mbClosed) {
		// Connection already closed, won't read
		LOG_ERROR("Attempted to read from closed connection.");
		return -1;
	}
	cbRead = recv(mConnectionDetails.iSock, reinterpret_cast<char*>(bufReceived), cbBuffer, 0);
	if (cbRead <= 0) {
		if (cbRead == 0) {
			LOG_DEBUG1("Connection closed by peer.");
		}
		else {
			LOG_ERROR("Socket read error.");
		}
		// Connection closed or error
		Close();
	}
	LOG_DEBUG0("Read %d bytes", cbRead);
	return cbRead;
}

int32_t TCPConnection::ReadAll(uint8_t* bufReceived, int32_t cbMinRead, int32_t cbMaxRead)
{
	// Bytes received so far
	int32_t cbReceived = 0;
	// Bytes received this iteration
	int32_t cbReceivedNow = 0;

	LOG_DEBUG0("Called.");
	if (cbMaxRead < cbMinRead) {
		cbMaxRead = cbMinRead;
	}
	while (cbReceived < cbMinRead) {
		cbReceivedNow = Read(bufReceived + cbReceived, cbMaxRead - cbReceived);
		if (cbReceivedNow == 0) {
			// Connection closed so return what we have
			LOG_DEBUG1("Connection closed by peer.");
			return cbReceived;
		}
		if (cbReceivedNow < 0) {
			// An error is an error
			LOG_ERROR("Socket read error.");
			return cbReceivedNow;
		}
		cbReceived += cbReceivedNow;
	}
	LOG_DEBUG0("Total read %d bytes", cbReceived);
	return cbReceived;
}

int32_t TCPConnection::Write(const uint8_t* bufSend, int32_t cbData)
{
	// Bytes written
	int32_t cbWritten = 0;

	LOG_DEBUG0("Called.");
	if (mbClosed) {
		// Connection already closed, won't read
		return -1;
	}
	cbWritten = send(mConnectionDetails.iSock, reinterpret_cast<const char*>(bufSend), cbData, 0);
	if (cbWritten <= 0) {
		if (cbWritten == 0) {
			LOG_DEBUG1("Connection closed by peer.");
		}
		else {
			LOG_ERROR("Socket write error.");
		}
		// Connection closed or error
		Close();
	}
	return cbWritten;
}

int32_t TCPConnection::WriteAll(const uint8_t* bufSend, int32_t cbData)
{
	// Bytes received so far
	int32_t cbSent = 0;
	// Bytes received this iteration
	int32_t cbSentNow = 0;

	LOG_DEBUG0("Called.");
	while (cbSent < cbData) {
		cbSentNow = Write(bufSend + cbSent, cbData - cbSent);
		if (cbSentNow == 0) {
			// Connection closed so return what we have
			LOG_DEBUG1("Connection closed by peer.");
			return cbSent;
		}
		if (cbSentNow < 0) {
			// An error is an error
			LOG_ERROR("Socket write error.");
			return cbSentNow;
		}
		cbSent += cbSentNow;
	}
	LOG_DEBUG0("Total written %d bytes", cbSent);
	return cbSent;
}

void TCPConnection::Close()
{
	LOG_DEBUG0("Called.");
	if (mbClosed == false) {
		LOG_INFO("Closing TCP connection to %s.", inet_ntoa(mConnectionDetails.BindDetails.sin_addr));
		shutdown(mConnectionDetails.iSock, SHUT_RDWR);
		closesocket(mConnectionDetails.iSock);
		mbClosed = true;
	}
}
