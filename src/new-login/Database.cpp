/**
 *	@file Database.cpp
 *	Database access and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "Database.h"
#include "Debugging.h"
#include <stdexcept>

DatabasePtr Database::smpSingletonObj = NULL;

mariadb::connection_ref Database::GetDatabase()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access database before initialzing.");
        throw std::runtime_error("Database not initialized.");
    }
    return smpSingletonObj->mpConnection;
}

std::mutex* Database::GetMutex()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access database before initialzing.");
        throw std::runtime_error("Database not initialized.");
    }
    return &smpSingletonObj->mMutex;
}

DatabasePtr Database::GetInstance()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access database before initialzing.");
        throw std::runtime_error("Database not initialized.");
    }
    return smpSingletonObj;
}

DatabasePtr Database::Initialize(const char* pszServer,
    uint16_t    wPort,
    const char* pszUsername,
    const char* pszPassword,
    const char* pszDatabase)
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj != NULL) {
        LOG_CRITICAL("Attempted to initialize the database twice.");
        throw std::runtime_error("Database already initialized.");
    }
    LOG_DEBUG1("Connecting to database.");
    smpSingletonObj = new Database(pszServer, wPort, pszUsername, pszPassword, pszDatabase);
    LOG_DEBUG1("Successfully connected.");
    return smpSingletonObj;
}

void Database::Destroy()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj != NULL) {
        LOG_DEBUG1("Disconnecting from database.");
        delete smpSingletonObj;
    }
}

Database::Database(const char* pszServer,
    uint16_t    wPort,
    const char* pszUsername,
    const char* pszPassword,
    const char* pszDatabase)
{
    LOG_DEBUG0("Called.");
    mpAccount = mariadb::account::create(std::string(pszServer), std::string(pszUsername), std::string(pszPassword), std::string(pszDatabase), wPort);
    if (mpAccount == NULL) {
        LOG_CRITICAL("Unable to create account object.");
        throw std::runtime_error("Account object creation failed.");
    }
    mpAccount->set_auto_commit(true);
    mpConnection = mariadb::connection::create(mpAccount);
    if (mpConnection == NULL) {
        LOG_CRITICAL("Could not connect to database.");
        throw std::runtime_error("Could not connect to database.");
    }
}
