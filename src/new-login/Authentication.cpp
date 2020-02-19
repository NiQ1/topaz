/**
 *	@file Authentication.cpp
 *	User authentication routines
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include <mariadb++/connection.hpp>
#include "Authentication.h"
#include "Database.h"
#include "GlobalConfig.h"
#include "Utilities.h"
#include "Debugging.h"
#include "SessionTracker.h"
#include <mutex>
#include <time.h>

Authentication::Authentication(std::shared_ptr<TCPConnection> Connection) : mLastError(AUTH_SUCCESS), mpConnection(Connection)
{
    LOG_DEBUG0("Called.");
}

uint32_t Authentication::AuthenticateUser(const char* pszUsername, const char* pszPassword)
{
    LOG_DEBUG0("Called.");
    try {
        LOCK_DB;
        DBConnection DB = Database::GetDatabase();
        GlobalConfigPtr Config = GlobalConfig::GetInstance();

        std::string strSqlQueryFmt("SELECT id, privileges FROM %saccounts WHERE username='%s' AND password=SHA2(CONCAT('%s', salt), 256)");
        std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pszUsername).c_str(),
            Database::RealEscapeString(pszPassword).c_str()));
        mariadb::result_set_ref pAccountsFound = DB->query(strSqlFinalQuery);
        if (pAccountsFound->row_count() == 0) {
            // Nothing found == unauthenticated (doesn't matter whether user doesn't exist or wrong password)
            mLastError = AUTH_NO_USER_OR_BAD_PASSWORD;
            return 0;
        }
        pAccountsFound->next();
        uint32_t dwPrivileges = pAccountsFound->get_unsigned32(1);
        if ((dwPrivileges & ACCT_PRIV_ENABLED) == 0) {
            mLastError = AUTH_ACCOUNT_DISABLED;
            return 0;
        }
        mLastError = AUTH_SUCCESS;
        uint32_t dwAccountId = pAccountsFound->get_unsigned32(0);
        // Add this account to the session tracker, which will allow the client
        // to connect to the data server.
        std::shared_ptr<LoginSession> NewSession = SessionTracker::GetInstance()->InitializeNewSession(dwAccountId,
            mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr,
            GlobalConfig::GetInstance()->GetConfigUInt("session_timeout"));
        NewSession->SetPrivilegesBitmask(dwPrivileges);
        return dwAccountId;
    }
    catch(...) {
        LOG_ERROR("Exception thrown on DB access.");
    }
    mLastError = AUTH_INTERNAL_FAILURE;
    return 0;
}

uint32_t Authentication::CreateUser(const char* pszUsername, const char* pszPassword, const char* pszEmail)
{
    LOG_DEBUG0("Called.");
    try {
        LOCK_DB;
        DBConnection DB = Database::GetDatabase();
        GlobalConfigPtr Config = GlobalConfig::GetInstance();

        // First make sure username is unique
        std::string strSqlQueryFmt("SELECT id FROM %saccounts WHERE username='%s';");
        std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pszUsername).c_str()));
        mariadb::result_set_ref pAccountsFound = DB->query(strSqlFinalQuery);
        if (pAccountsFound->row_count() != 0) {
            // User name already taken
            mLastError = AUTH_USERNAME_TAKEN;
            return 0;
        }
        // Then make sure the user is not using "1234" or something dumb like that as password
        if (!CheckPasswordComplexity(pszPassword)) {
            mLastError = AUTH_PASSWORD_TOO_WEAK;
            return 0;
        }
        // Random salt automatically added so two identical passwords won't have the same hash
        std::string strSalt(GenerateSalt());
        // Showtime
        if (pszEmail) {
            strSqlQueryFmt = "INSERT INTO %saccounts (username, password, salt, email) VALUES ('%s', SHA2(CONCAT('%s', '%s'), 256), '%s', '%s')";
            strSqlFinalQuery = FormatString(&strSqlQueryFmt,
                Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
                Database::RealEscapeString(pszUsername).c_str(),
                Database::RealEscapeString(pszPassword).c_str(),
                Database::RealEscapeString(strSalt).c_str(),
                Database::RealEscapeString(strSalt).c_str(),
                Database::RealEscapeString(pszEmail).c_str());
        }
        else {
            strSqlQueryFmt = "INSERT INTO %saccounts (username, password, salt) VALUES ('%s', SHA2(CONCAT('%s', '%s'), 256), '%s')";
            strSqlFinalQuery = FormatString(&strSqlQueryFmt,
                Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
                Database::RealEscapeString(pszUsername).c_str(),
                Database::RealEscapeString(pszPassword).c_str(),
                Database::RealEscapeString(strSalt).c_str(),
                Database::RealEscapeString(strSalt).c_str());
            }
        if (DB->insert(strSqlFinalQuery) == 0) {
            // Failed
            mLastError = mLastError = AUTH_INTERNAL_FAILURE;
            return 0;
        }
        // Now pull the id of the user we've just created
        strSqlQueryFmt = "SELECT id FROM %saccounts WHERE username='%s';";
        pAccountsFound = DB->query(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pszUsername).c_str()));
        if (pAccountsFound->row_count() == 0) {
            // Shouldn't happen
            mLastError = AUTH_INTERNAL_FAILURE;
            return 0;
        }
        pAccountsFound->next();
        mLastError = AUTH_SUCCESS;
        uint32_t dwAccountId = pAccountsFound->get_unsigned32(0);
        // Add this account to the session tracker, which will allow the client
        // to connect to the data server.
        SessionTracker::GetInstance()->InitializeNewSession(dwAccountId,
            mpConnection->GetConnectionDetails().BindDetails.sin_addr.s_addr,
            GlobalConfig::GetInstance()->GetConfigUInt("session_timeout"));
        return dwAccountId;
    }
    catch (...) {
        LOG_ERROR("Exception thrown on DB access.");
        mLastError = AUTH_INTERNAL_FAILURE;
        return 0;
    }
}

bool Authentication::ChangePassword(const char* pszUsername, const char* pszOldPassword, const char* pszNewPassword)
{
    LOG_DEBUG0("Called.");
    try {
        LOCK_DB;
        DBConnection DB = Database::GetDatabase();
        GlobalConfigPtr Config = GlobalConfig::GetInstance();

        uint32_t dwUserUID = AuthenticateUser(pszUsername, pszOldPassword);
        if ((dwUserUID == 0) && (mLastError != AUTH_ACCOUNT_DISABLED)) {
            // mLastError already set by AuthenticateUser
            // Note: For security reasons, disabled accounts are still allowed to change their passwords
            return false;
        }
        if (!CheckPasswordComplexity(pszNewPassword)) {
            mLastError = AUTH_PASSWORD_TOO_WEAK;
            return false;
        }
        std::string strSalt(GenerateSalt());
        std::string strSqlQueryFmt("UPDATE %saccounts SET password=SHA2(CONCAT('%s', '%s'), 256), salt='%s' WHERE id=%d;");
        if (DB->execute(FormatString(&strSqlQueryFmt,
            Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str(),
            Database::RealEscapeString(pszNewPassword).c_str(),
            Database::RealEscapeString(strSalt).c_str(),
            Database::RealEscapeString(strSalt).c_str(),
            dwUserUID)) == 0) {
            mLastError = AUTH_INTERNAL_FAILURE;
            return false;
        }
        mLastError = AUTH_SUCCESS;
        return true;
    }
    catch (...) {
        LOG_ERROR("Exception thrown on DB access.");
        mLastError = AUTH_INTERNAL_FAILURE;
        return false;
    }
}

Authentication::AUTHENTICATION_ERROR Authentication::GetLastAuthenticationError() const
{
    return mLastError;
}

std::string Authentication::GenerateSalt()
{
    LOG_DEBUG0("Called.");
    char szRandomChars[11];
    for (int i = 0; i < sizeof(szRandomChars) - 1; i++) {
        // Generate 10 printable characters (range 33-126)
        szRandomChars[i] = (rand() % 93) + 33;
    }
    szRandomChars[sizeof(szRandomChars) - 1] = '\0';
    return std::string(szRandomChars) + std::to_string(time(NULL));
}

bool Authentication::CheckPasswordComplexity(const char* pszPassword)
{
    LOG_DEBUG0("Called.");
    size_t sPassLen = strlen(pszPassword);
    if (sPassLen < 8) {
        return false;
    }
    uint8_t cHasUppercase = 0;
    uint8_t cHasLowercase = 0;
    uint8_t cHasNumerics = 0;
    uint8_t cHasOthers = 0;
    for (size_t i = 0; i < sPassLen; i++) {
        if ((pszPassword[i] >= 'A') && (pszPassword[i] <= 'Z')) {
            cHasUppercase = 1;
        }
        else if ((pszPassword[i] >= 'a') && (pszPassword[i] <= 'z')) {
            cHasLowercase = 1;
        }
        else if ((pszPassword[i] >= '0') && (pszPassword[i] <= '9')) {
            cHasNumerics = 1;
        }
        else {
            cHasOthers = 1;
        }
    }
    return (cHasUppercase + cHasLowercase + cHasNumerics + cHasOthers) >= 3;
}
