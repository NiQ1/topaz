/**
 *	@file Authentication.h
 *	User authentication routines
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_AUTHENTICATION_H
#define FFXI_LOGIN_AUTHENTICATION_H

#include "TCPConnection.h"
#include <stdint.h>

/**
 *  Create one instance of this for each user that needs to authenticate.
 */
class Authentication
{
public:

    /**
     *  Initialize the authenticator.
     */
    Authentication();

    /**
     *  Possible errors returned by the authentication process
     */
    enum AUTHENTICATION_ERROR
    {
        AUTH_SUCCESS = 0,
        AUTH_NO_USER_OR_BAD_PASSWORD = 1,
        AUTH_USERNAME_TAKEN = 2,
        AUTH_PASSWORD_TOO_WEAK = 3,
        AUTH_INTERNAL_FAILURE = 4,
        AUTH_ACCOUNT_DISABLED = 5,
        AUTH_LAST
    };

    /**
     *  Authenticate user by username / password.
     *  @param pszUsername Username to authenticate
     *  @param pszPassword Password to authenticate
     *  @return Account ID of the user on success or 0 on failure.
     */
    uint32_t AuthenticateUser(const char* pszUsername, const char* pszPassword);

    /**
     *  Create a new user account.
     *  @param pszUsername Username to create
     *  @param pszPassword Password to assign
     *  @return Account ID of the user on success or 0 on failure.
     */
    uint32_t CreateUser(const char* pszUsername, const char* pszPassword);

    /**
     *  Changes the password of an existing user.
     *  @param pszUsername User to modify
     *  @param pszOldPassword Existing password
     *  @param pszNewPassword New password to set
     *  @return true if successful, false on failure.
     */
    bool ChangePassword(const char* pszUsername, const char* pszOldPassword, const char* pszNewPassword);

    /**
     *  Negotiate authentication with the user connected to the given TCP
     *  connection.
     *  @param Connection Open TCP connection to the client being authenticated
     *  @return Account ID of the user on success or 0 on failure
     */
    uint32_t Run(TCPConnection& Connection);

    /**
     *  Return the value of the last authentication error.
     *  @return Authentication error code (see enum)
     */
    AUTHENTICATION_ERROR GetLastAuthenticationError() const;

private:

    /// Last authentication error that occured
    AUTHENTICATION_ERROR mLastError;

    /**
     *  Generate a unique string to be used as password salt.
     *  This is not cryptographically secure, just needs to be different on each call.
     *  @return Unique string to be used as salt
     */
    std::string GenerateSalt();

    /**
     *  Check whether a given password meet the following creteria -
     *  * At least 8 characters
     *  * Must have at least 3 of - Uppercase chars, lowercase chars, numerics, symbols
     *  @param pszPassword Password to check
     *  @return true if the password is complex enough, false if too weak
     */
    bool CheckPasswordComplexity(const char* pszPassword);
};

#endif
