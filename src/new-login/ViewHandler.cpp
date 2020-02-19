/**
 *	@file ViewHandler.h
 *	Implementation of the view protocol
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "ViewHandler.h"
#include "Debugging.h"
#include "SessionTracker.h"
#include <time.h>

ViewHandler::ViewHandler(std::shared_ptr<TCPConnection> connection) : ProtocolHandler(connection)
{
    LOG_DEBUG0("Called.");
}

ViewHandler::~ViewHandler()
{
    LOG_DEBUG0("Called.");
}

void ViewHandler::Run()
{
    LOG_DEBUG0("Called.");

    // The account ID is not sent on the view port, which is very unfortunate
    // we have to fall back to the client's IP address and hope two people
    // don't connect too quickly.
    auto pSessionTracker = SessionTracker::GetInstance();
    std::shared_ptr<LoginSession> pSession;
    try {
        pSession = pSessionTracker->LookupSessionByIP(mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr);
    }
    catch (...) {
        LOG_WARNING("Unknown user attempted to connect to view port.");
        mpConnection->Close();
        return;
    }
    // Don't catch this session again when performing IP lookups
    pSession->SetIgnoreIPLookupFlag(true);
    // Add more time to the user, if creating a new character they may stay
    // connected to the view server for a while.
    pSession->SetExpiryTimeRelative(600);
}