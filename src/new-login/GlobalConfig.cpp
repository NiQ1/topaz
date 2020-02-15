/**
 *	@file GlobalConfig.cpp
 *	Reads and stores the global configuration
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "GlobalConfig.h"
#include "Debugging.h"

GlobalConfig::GlobalConfig()
{
    LOG_INFO("Using default configuration file: " DEFAULT_CONFIG_FILE_NAME);
    mhConfigFile = fopen(DEFAULT_CONFIG_FILE_NAME, "r");
    if (mhConfigFile == NULL) {
        LOG_CRITICAL("Could not open configuration file.");
        throw std::runtime_error("Could not open configuration file.");
    }
}

GlobalConfig::GlobalConfig(std::string& strConfigFileName)
{
    LOG_INFO("Using configuration file: %s", strConfigFileName.c_str());
    mhConfigFile = fopen(strConfigFileName.c_str(), "r");
    if (mhConfigFile == NULL) {
        LOG_CRITICAL("Could not open configuration file.");
        throw std::runtime_error("Could not open configuration file.");
    }
}

GlobalConfig::~GlobalConfig()
{
    Destroy();
}

std::string GlobalConfig::GetConfigString(std::string& strConfigName)
{
    LOG_DEBUG0("Called.");
    // May already be cached
    LOG_DEBUG1("Searching configuration for: %s", strConfigName.c_str());
    auto strVal = mmapStringVals.find(strConfigName);
    if (strVal != mmapStringVals.end()) {
        LOG_DEBUG1("Value found in cache: %s", strVal->first);
        return strVal->first;
    }
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
            LOG_DEBUG1("Value found in file: %s", configVal);
            return configVal;
        }
        linenum++;
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

int32_t GlobalConfig::GetConfigInt(std::string& strConfigName)
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

uint32_t GlobalConfig::GetConfigUInt(std::string& strConfigName)
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
    if (smSingletonObj == NULL) {
        smSingletonObj = new GlobalConfig();
    }
    return smSingletonObj;
}

GlobalConfigPtr GlobalConfig::GetInstance(std::string& strConfigFileName)
{
    if (smSingletonObj == NULL) {
        smSingletonObj = new GlobalConfig(strConfigFileName);
    }
    return smSingletonObj;
}

void GlobalConfig::Destroy()
{
    if (smSingletonObj == NULL) {
        return;
    }
    if (smSingletonObj->mhConfigFile) {
        fclose(smSingletonObj->mhConfigFile);
        smSingletonObj->mhConfigFile = NULL;
    }
    delete smSingletonObj;
    smSingletonObj = NULL;
}

void GlobalConfig::trim(std::string& str)
{
    size_t pos = 0;
    pos = str.find_first_not_of(' ');
    if (pos != std::string::npos) {
        str.erase(0, pos);
    }
    pos = str.find_last_not_of(' ');
    if (pos != std::string::npos) {
        str.erase(pos + 1);
    }
}

std::string GlobalConfig::GetDefaultValue(std::string& strConfigName)
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
    LOG_ERROR("No default configuration value found.");
    throw std::runtime_error("Configuration value does not have a hardcoded default");
}
