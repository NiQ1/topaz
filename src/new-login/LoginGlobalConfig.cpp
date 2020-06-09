/**
 *	@file GlobalConfig.cpp
 *	Reads and stores the global configuration
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "LoginGlobalConfig.h"
#include "new-common/Debugging.h"

LoginGlobalConfig::LoginGlobalConfig() : GlobalConfig(std::string(DEFAULT_CONFIG_FILE_NAME))
{
    LOG_DEBUG0("Called.");
}

LoginGlobalConfig::LoginGlobalConfig(const std::string& strConfigFileName) : GlobalConfig(strConfigFileName)
{
    LOG_DEBUG0("Called.");
}

LoginGlobalConfig::~LoginGlobalConfig()
{
}

GlobalConfigPtr LoginGlobalConfig::GetInstance()
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new LoginGlobalConfig(std::string(DEFAULT_CONFIG_FILE_NAME));
    }
    return smpSingletonObj;
}

GlobalConfigPtr LoginGlobalConfig::GetInstance(const std::string& strConfigFileName)
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new LoginGlobalConfig(strConfigFileName);
    }
    return smpSingletonObj;
}

std::string LoginGlobalConfig::GetDefaultValue(const std::string& strConfigName)
{
    LOG_DEBUG0("Called.");
    if (strConfigName == "db_server") {
        return "127.0.0.1";
    }
    else if (strConfigName == "db_port") {
        return "3306";
    }
    else if (strConfigName == "db_database") {
        return "topaz_login";
    }
    else if (strConfigName == "db_username") {
        return "topaz";
    }
    else if (strConfigName == "db_password") {
        return "topaz";
    }
    else if (strConfigName == "db_prefix") {
        return "";
    }
    else if (strConfigName == "auth_port") {
        return "54231";
    }
    else if (strConfigName == "data_port") {
        return "54230";
    }
    else if (strConfigName == "login_ip") {
        return "0.0.0.0";
    }
    else if (strConfigName == "password_hash_secret") {
        // Secret to add to password hashes, change this to something random
        return "";
    }
    else if (strConfigName == "new_account_content_ids") {
        // Number of content ids to associate with new accounts
        return "3";
    }
    else if (strConfigName == "max_login_attempts") {
        // Max number of login attempts before the client is disconnected
        return "3";
    }
    else if (strConfigName == "max_client_connections") {
        // Max number of concurrent connections a single client
        // can have open. Note - Each client needs at least 3 concurrent
        // connections (auth, data and view)
        return "10";
    }
    else if (strConfigName == "session_timeout") {
        return "30";
    }
    else if (strConfigName == "expected_client_version") {
        return "30191004_0";
    }
    else if (strConfigName == "version_lock") {
        // 0 - No version lock
        // 1 - Version lock, only expected client version can connect
        // 2 - One way version lock, expected client version or greater can connect
        return "0";
    }
    LOG_ERROR("No default configuration value found.");
    throw std::runtime_error("Configuration value does not have a hardcoded default");
}
