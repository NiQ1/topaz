/**
 *	@file LoginMain.cpp
 *	Main body and initialization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "Debugging.h"
#include "Database.h"
#include "GlobalConfig.h"
#include "LoginServer.h"
#include "ProtocolFactory.h"
#include "SessionTracker.h"

#include <signal.h>
#include <time.h>
#include <thread>
#include <chrono>
#include <string>

#ifdef _WIN32
#include <io.h>
#define close _close
#define fileno _fileno
#else
#include <unistd.h>
#endif

bool gbExitFlag = false;

void LoginSignalHandler(int signal)
{
    gbExitFlag = true;
    // If we do a UI then the main thread is likely to be blocked reading
    // from the keyboard. Close stdin so the signal can be processed.
    close(fileno(stdin));
}

int main(int argc, char* argv[])
{
    LOG_INFO("Starting.");
    signal(SIGTERM, LoginSignalHandler);
    signal(SIGINT, LoginSignalHandler);
    srand(static_cast<unsigned int>(time(NULL)));

    // Load global configuration
    GlobalConfigPtr Config = GlobalConfig::GetInstance();
    // Connect to database
    DatabasePtr DB = Database::Initialize(Config->GetConfigString("db_server").c_str(),
        Config->GetConfigUInt("db_port"),
        Config->GetConfigString("db_username").c_str(),
        Config->GetConfigString("db_password").c_str(),
        Config->GetConfigString("db_database").c_str());
    SessionTracker::GetInstance();

    // Login server handles authentication
    LoginServer LoginServerInstance;
    LoginServerInstance.AddBind(ProtocolFactory::PROTOCOL_AUTH, Config->GetConfigUInt("auth_port"));
    LoginServerInstance.StartThread();

    LOG_INFO("Initialization complete, server is running.");

    while (gbExitFlag == false) {
        // TODO: Maybe should add some UI code here
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOG_INFO("Shutting down server.");

    LoginServerInstance.Shutdown();
    SessionTracker::GetInstance()->Destroy();
    DB->Destroy();
    Config->Destroy();

    LOG_INFO("Shutdown complete.");
}