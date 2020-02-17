/**
 *	@file Database.h
 *	Database access and synchronization
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_DATABASE_H
#define FFXI_LOGIN_DATABASE_H

#include <stddef.h>
#include <mutex>
#include <memory>

class Database;
typedef Database* DatabasePtr;

// Easy way to lock the DB mutex
#define LOCK_DB std::lock_guard<std::mutex> l(*Database::GetMutex())

// Hack around a bug in MariaDB++ which causes a compile error on Windows
// if WinSock2.h is included before its headers.
namespace mariadb
{
    class connection;
    class account;
};
typedef std::shared_ptr<mariadb::connection> DBConnection;
typedef std::shared_ptr<mariadb::account> DBAccount;

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
    static DBConnection GetDatabase();

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

    /**
     *  Basically same as mysql_real_escape_string but doesn't need
     *  a connection handle, which MariaDB++ doesn't expose.
     *  @param strString string to escape
     *  @return Escaped string
     */
    static std::string RealEscapeString(const std::string& strString);

    /**
     *  Destructor. It is preferrable to call destroy explicitly.
     */
    ~Database();

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
    static DatabasePtr smpSingletonObj;
    /// Object is being destroyed
    static bool sbBeingDestroyed;

    /// MariaDB++ connection handle
    DBConnection mpConnection;
    /// MariaDB++ account handle
    DBAccount mpAccount;
    /// Database access mutex
    std::mutex mMutex;
};

#endif
