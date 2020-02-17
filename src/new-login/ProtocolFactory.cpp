/**
 *	@file ProtocolFactory.cpp
 *	Creates instances of protocol handlers by given type
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "ProtocolFactory.h"
#include "AuthHandler.h"
#include "Debugging.h"
#include <stdexcept>

ProtocolHandler* ProtocolFactory::BuildHandler(LOGIN_PROTOCOLS protocol, std::shared_ptr<TCPConnection> connection)
{
    LOG_DEBUG0("Called.");
    switch (protocol)
    {
    case PROTOCOL_AUTH:
        LOG_DEBUG0("Constructing authentication handler.");
        return new AuthHandler(connection);
    }
    LOG_CRITICAL("Protocol factory called with unknown or unsupported protocol.");
    throw std::logic_error("Invalid or unsupported protocol");
}
