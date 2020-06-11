/**
 *	@file WorldGlobalConfig.cpp
 *	Reads and stores the global configuration
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "WorldGlobalConfig.h"
#include "new-common/Debugging.h"

WorldGlobalConfig::WorldGlobalConfig() : GlobalConfig(std::string(WORLD_DEFAULT_CONFIG_FILE_NAME))
{
    LOG_DEBUG0("Called.");
}

WorldGlobalConfig::WorldGlobalConfig(const std::string& strConfigFileName) : GlobalConfig(strConfigFileName)
{
    LOG_DEBUG0("Called.");
}

WorldGlobalConfig::~WorldGlobalConfig()
{
}

GlobalConfigPtr WorldGlobalConfig::GetInstance()
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new WorldGlobalConfig(std::string(WORLD_DEFAULT_CONFIG_FILE_NAME));
    }
    return smpSingletonObj;
}

GlobalConfigPtr WorldGlobalConfig::GetInstance(const std::string& strConfigFileName)
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new WorldGlobalConfig(strConfigFileName);
    }
    return smpSingletonObj;
}

std::string WorldGlobalConfig::GetDefaultValue(const std::string& strConfigName)
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
    else if (strConfigName == "mq_server") {
        return "127.0.0.1";
    }
    else if (strConfigName == "mq_port") {
        return "5672";
    }
    else if (strConfigName == "mq_ssl") {
        return "0";
    }
    else if (strConfigName == "mq_ssl_verify") {
        return "0";
    }
    else if (strConfigName == "mq_ssl_ca_file") {
        return "";
    }
    else if (strConfigName == "mq_ssl_client_cert") {
        return "";
    }
    else if (strConfigName == "mq_ssl_client_key") {
        return "";
    }
    else if (strConfigName == "mq_username") {
        return "topaz";
    }
    else if (strConfigName == "mq_password") {
        return "topaz";
    }
    else if (strConfigName == "mq_vhost") {
        return "topaz";
    }
    LOG_ERROR("No default configuration value found.");
    throw std::runtime_error("Configuration value does not have a hardcoded default");
}
