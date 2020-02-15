/**
 *	@file Database.h
 *	Database access and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_DATABASE_H
#define FFXI_LOGIN_DATABASE_H

#include <mariadb++/account.hpp>
#include <mariadb++/connection.hpp>
#include <stddef.h>
#include <mutex>
#include <memory>

class Database;
typedef Database* DatabasePtr;

/**
 *  Database access singleton class
 */
class Database
{
public:

    /**
     *  Return the database connection object.
     *  @return Singleton MariaDB++ connection object
     */
    static mariadb::connection_ref GetDatabase();

    /**
     *  Gets the global database Mutex object. Lock this before
     *  any database access.
     *  @return Database mutex object.
     */
    static std::mutex* GetMutex();

    /**
     *  Return an instance to the singleton.
     *  @return Singleton instance of this class.
     */
    static DatabasePtr GetInstance();

    /**
     *  Initialize the DB connection, should be called only once.
     *  @param pszServer Server IP address
     *  @param wPort Server port number
     *  @param pszUsername Username to login as
     *  @param pszPassword Password of the given user
     *  @param pszDatabase Working database
     *  @return Singleton instance of this class.
     */
    static DatabasePtr Initialize(const char* pszServer,
        uint16_t    wPort,
        const char* pszUsername,
        const char* pszPassword,
        const char* pszDatabase);

    /**
     *  Disconnect from the database and destroy the singleton,
     *  should only be called when the server is shutting down.
     */
    void Destroy();

private:

    /**
     *  Private constructor for the singleton object
     *  @param pszServer Server IP address
     *  @param wPort Server port number
     *  @param pszUsername Username to login as
     *  @param pszPassword Password of the given user
     *  @param pszDatabase Working database
     */
    Database(const char* pszServer,
        uint16_t    wPort,
        const char* pszUsername,
        const char* pszPassword,
        const char* pszDatabase);

    /// Static database singleton pointer
    static DatabasePtr smpDatabase;

    /// MariaDB++ connection handle
    mariadb::connection_ref mpConnection;
    /// MariaDB++ account handle
    mariadb::account_ref mpAccount;
    /// Database access mutex
    std::mutex mMutex;
};

#endif
