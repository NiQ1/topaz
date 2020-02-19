/**
 *	@file GlobalData.cpp
 *	Global data structures and objects
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "GlobalData.h"
#include "Debugging.h"
#include <mariadb++/connection.hpp>
#include "Database.h"
#include "GlobalConfig.h"
#include "Utilities.h"

GlobalDataPtr GlobalData::smpSingletonObj = NULL;
bool GlobalData::sbBeingDestroyed = false;

GlobalData::GlobalData() : mbWorldListLoaded(false)
{
    LOG_DEBUG0("Called.");
}

GlobalData::~GlobalData()
{
    if (sbBeingDestroyed == false) {
        Destroy();
    }
}

GlobalDataPtr GlobalData::GetInstance()
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new GlobalData();
    }
    return smpSingletonObj;
}


std::mutex* GlobalData::GetMutex()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access config before initialzing.");
        throw std::runtime_error("Config not initialized.");
    }
    return &smpSingletonObj->mMutex;
}

void GlobalData::Destroy()
{
    LOG_DEBUG0("Called.");
    sbBeingDestroyed = true;
    if (smpSingletonObj == NULL) {
        return;
    }
    delete smpSingletonObj;
    smpSingletonObj = NULL;
}

std::string GlobalData::GetWorldName(uint32_t dwWorldID)
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    auto WorldEnt = mmapWorldList.find(dwWorldID);
    if (WorldEnt != mmapWorldList.end()) {
        LOG_ERROR("World ID not found in list.");
        throw std::runtime_error("World ID not found.");
    }
    return std::string(WorldEnt->second.szWorldName);
}

void GlobalData::LoadWorlds()
{
    LOG_DEBUG0("Called.");
    LOCK_GLOBDATA;

    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();
    LOCK_DB;
    LOCK_CONFIG;

    std::string strSqlQueryFmt("SELECT id, name, mq_server_ip, mq_server_port FROM %sworlds WHERE is_active=1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str()));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    if (pResultSet->row_count() == 0) {
        LOG_ERROR("Failed to query the world list.");
        throw std::runtime_error("world list query failed.");
    }
    WORLD_ENTRY NewWorld = { 0 };
    while (pResultSet->next()) {
        memset(&NewWorld, 0, sizeof(NewWorld));
        NewWorld.dwWorldId = pResultSet->get_unsigned32(0);
        strncpy(NewWorld.szWorldName, pResultSet->get_string(1).c_str(), sizeof(NewWorld.szWorldName) - 1);
        strncpy(NewWorld.szMQIP, pResultSet->get_string(2).c_str(), sizeof(NewWorld.szMQIP) - 1);
        NewWorld.wMQPort = pResultSet->get_unsigned16(3);
        NewWorld.bIsTestWorld = pResultSet->get_boolean(4);
        mmapWorldList[NewWorld.dwWorldId] = NewWorld;
    }
}
