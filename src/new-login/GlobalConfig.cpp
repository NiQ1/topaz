/**
 *	@file GlobalConfig.cpp
 *	Reads and stores the global configuration
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "GlobalConfig.h"
#include "Debugging.h"

GlobalConfigPtr GlobalConfig::smpSingletonObj = NULL;
bool GlobalConfig::sbBeingDestroyed = false;

GlobalConfig::GlobalConfig()
{
    LOG_INFO("Using default configuration file: " DEFAULT_CONFIG_FILE_NAME);
    mhConfigFile = fopen(DEFAULT_CONFIG_FILE_NAME, "r");
    if (mhConfigFile == NULL) {
        LOG_WARNING("Could not open configuration file, falling back to default values.");
    }
}

GlobalConfig::GlobalConfig(std::string& strConfigFileName)
{
    LOG_INFO("Using configuration file: %s", strConfigFileName.c_str());
    mhConfigFile = fopen(strConfigFileName.c_str(), "r");
    if (mhConfigFile == NULL) {
        LOG_WARNING("Could not open configuration file, falling back to default values.");
    }
}

GlobalConfig::~GlobalConfig()
{
    if (sbBeingDestroyed == false) {
        Destroy();
    }
}

std::string GlobalConfig::GetConfigString(const char* pszConfigName)
{
    return GetConfigString(std::string(pszConfigName));
}

std::string GlobalConfig::GetConfigString(const std::string& strConfigName)
{
    LOG_DEBUG0("Called.");
    LOCK_CONFIG;
    // May already be cached
    LOG_DEBUG1("Searching configuration for: %s", strConfigName.c_str());
    auto strVal = mmapStringVals.find(strConfigName);
    if (strVal != mmapStringVals.end()) {
        LOG_DEBUG1("Value found in cache: %s", strVal->first);
        return strVal->second;
    }
    if (mhConfigFile) {
        LOG_DEBUG0("Value not in cache, searching file.");
        // Not found in cache so read from file
        fseek(mhConfigFile, 0, SEEK_SET);
        // Should be enough to hold a single line
        char configFileLine[1024] = { 0 };
        // Lines are in the format name=value
        char* equalSignPos = NULL;
        std::string configName;
        std::string configVal;
        // Keep track of configuration line numbers for logging
        int linenum = 0;
        while (fgets(configFileLine, sizeof(configFileLine) - 1, mhConfigFile) != NULL) {
            if (configFileLine[0] == ';') {
                // Skip comments
                continue;
            }
            equalSignPos = strchr(configFileLine, '=');
            if (equalSignPos == NULL) {
                LOG_WARNING("Skipping malformed configuration line: %d.", linenum);
                continue;
            }
            // Break into name and value
            *equalSignPos = '\0';
            equalSignPos++;
            configName = configFileLine;
            configVal = equalSignPos;
            // Trim whitespaces
            trim(configName);
            trim(configVal);
            if (configName == strConfigName) {
                // Found it, add to cache
                mmapStringVals[strConfigName] = configVal;
                LOG_DEBUG1("Value found in file: %s", configVal.c_str());
                return configVal;
            }
            linenum++;
        }
    }
    // Fallback to default value
    try {
        return GetDefaultValue(strConfigName);
    }
    catch (std::runtime_error&) {
    }
    LOG_ERROR("Configuration value %s does not exist.", strConfigName);
    throw std::runtime_error("Missing configuration value.");
}

int32_t GlobalConfig::GetConfigInt(const char* pszConfigName)
{
    return GetConfigInt(std::string(pszConfigName));
}

int32_t GlobalConfig::GetConfigInt(const std::string& strConfigName)
{
    LOG_DEBUG0("Called.");
    std::string configValAsString = GetConfigString(strConfigName);
    const char* pszConfigVal = configValAsString.c_str();
    // Used to verify that the value is a number
    char* pszEndNum = NULL;

    int32_t iIntVal = strtol(pszConfigVal, &pszEndNum, 10);
    if ((unsigned)(pszEndNum - pszConfigVal) > configValAsString.length()) {
        LOG_ERROR("Configuration value is not an integer.");
        throw std::runtime_error("Configuration value is not an integer.");
    }
    LOG_DEBUG1("Value found: %d", iIntVal);
    return iIntVal;
}

uint32_t GlobalConfig::GetConfigUInt(const char* pszConfigName)
{
    return GetConfigUInt(std::string(pszConfigName));
}

uint32_t GlobalConfig::GetConfigUInt(const std::string& strConfigName)
{
    LOG_DEBUG0("Called.");
    std::string configValAsString = GetConfigString(strConfigName);
    const char* pszConfigVal = configValAsString.c_str();
    // Used to verify that the value is a number
    char* pszEndNum = NULL;

    uint32_t uiIntVal = strtol(pszConfigVal, &pszEndNum, 10);
    if ((unsigned)(pszEndNum - pszConfigVal) > configValAsString.length()) {
        LOG_ERROR("Configuration value is not an unsigned integer.");
        throw std::runtime_error("Configuration value is not an unsigned integer.");
    }
    LOG_DEBUG1("Value found: %d", uiIntVal);
    return uiIntVal;
}

GlobalConfigPtr GlobalConfig::GetInstance()
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new GlobalConfig();
    }
    return smpSingletonObj;
}

GlobalConfigPtr GlobalConfig::GetInstance(std::string& strConfigFileName)
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new GlobalConfig(strConfigFileName);
    }
    return smpSingletonObj;
}

std::mutex* GlobalConfig::GetMutex()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access config before initialzing.");
        throw std::runtime_error("Config not initialized.");
    }
    return &smpSingletonObj->mMutex;
}

void GlobalConfig::Destroy()
{
    LOG_DEBUG0("Called.");
    sbBeingDestroyed = true;
    if (smpSingletonObj == NULL) {
        return;
    }
    if (smpSingletonObj->mhConfigFile) {
        fclose(smpSingletonObj->mhConfigFile);
        smpSingletonObj->mhConfigFile = NULL;
    }
    delete smpSingletonObj;
    smpSingletonObj = NULL;
}

void GlobalConfig::trim(std::string& str)
{
    size_t pos = 0;
    pos = str.find_first_not_of(" \r\n");
    if (pos != std::string::npos) {
        str.erase(0, pos);
    }
    pos = str.find_last_not_of(" \r\n");
    if (pos != std::string::npos) {
        str.erase(pos + 1);
    }
}

std::string GlobalConfig::GetDefaultValue(const std::string& strConfigName)
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
    else if (strConfigName == "login_ip") {
        return "0.0.0.0";
    }
    else if (strConfigName == "max_login_attempts") {
        // Max number of login attempts before the client is disconnected
        return "3";
    }
    else if (strConfigName == "max_client_connections") {
        // Max number of concurrent connections a single client
        // can have open.
        return "2";
    }
    else if (strConfigName == "session_timeout") {
        return "30";
    }
    LOG_ERROR("No default configuration value found.");
    throw std::runtime_error("Configuration value does not have a hardcoded default");
}
