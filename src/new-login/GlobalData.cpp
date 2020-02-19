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


std::recursive_mutex* GlobalData::GetMutex()
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
    LOCK_GLOBDATA;
    auto WorldEnt = mmapWorldList.find(dwWorldID);
    if (WorldEnt != mmapWorldList.end()) {
        LOG_ERROR("World ID not found in list.");
        throw std::runtime_error("World ID not found.");
    }
    return std::string(WorldEnt->second.szWorldName);
}

std::shared_ptr<uint8_t> GlobalData::GetAdminWorldsPacket()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_GLOBDATA;
    return mbufWorldsPacketAdmin;
}

uint32_t GlobalData::GetAdminWorldsPacketSize()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_GLOBDATA;
    return mdwWorldsPacketAdminSize;
}

std::shared_ptr<uint8_t> GlobalData::GetUserWorldsPacket()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_GLOBDATA;
    return mbufWorldsPacketUser;
}

uint32_t GlobalData::GetUserWorldsPacketSize()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_GLOBDATA;
    return mdwWorldsPacketUserSize;
}

void GlobalData::LoadWorlds()
{
    LOG_DEBUG0("Called.");
    if (mbWorldListLoaded) {
        LOG_DEBUG1("World list already loaded, not loading again.");
        return;
    }

    LOCK_GLOBDATA;

    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = GlobalConfig::GetInstance();
    LOCK_DB;
    LOCK_CONFIG;

    std::string strSqlQueryFmt("SELECT id, name, mq_server_ip, mq_server_port, is_test FROM %sworlds WHERE is_active=1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str()));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    uint32_t dwNumWorlds = pResultSet->row_count() == 0;
    if (dwNumWorlds) {
        LOG_CRITICAL("Failed to query the world list.");
        throw std::runtime_error("world list query failed.");
    }
    WORLD_ENTRY NewWorld = { 0 };
    // Build the world packets as well, since they are static we might as well do it only once
    // Note that packet size is dynamic (according to the number of worlds). The world list packet
    // consists of one unknown 4 byte header and then world list (4 byte world id, 15 byte world
    // name and 1 byte null terminator).
    mbufWorldsPacketAdmin = std::shared_ptr<uint8_t>(new uint8_t[sizeof(WORLD_PACKET_ENTRY) * dwNumWorlds + 4]);
    *reinterpret_cast<uint32_t*>(&(*mbufWorldsPacketAdmin)) = 0x20;
    WORLD_PACKET_ENTRY* pWorldsAdmin = reinterpret_cast<WORLD_PACKET_ENTRY*>((&(*mbufWorldsPacketAdmin)) + 4);
    mdwWorldsPacketAdminSize = 4;
    // We don't know what will be the final size of this but it's guaranteed not to be longer
    // than the admin packet.
    mbufWorldsPacketUser = std::shared_ptr<uint8_t>(new uint8_t[sizeof(WORLD_PACKET_ENTRY) * dwNumWorlds + 4]);
    *reinterpret_cast<uint32_t*>(&(*mbufWorldsPacketUser)) = 0x20;
    WORLD_PACKET_ENTRY* pWorldsUser = reinterpret_cast<WORLD_PACKET_ENTRY*>((&(*mbufWorldsPacketUser)) + 4);
    mdwWorldsPacketUserSize = 4;
    uint32_t dwAdminWorlds = 0;
    uint32_t dwUserWorlds = 0;
    while (pResultSet->next()) {
        memset(&NewWorld, 0, sizeof(NewWorld));
        NewWorld.dwWorldId = pResultSet->get_unsigned32(0);
        strncpy(NewWorld.szWorldName, pResultSet->get_string(1).c_str(), sizeof(NewWorld.szWorldName) - 1);
        strncpy(NewWorld.szMQIP, pResultSet->get_string(2).c_str(), sizeof(NewWorld.szMQIP) - 1);
        NewWorld.wMQPort = static_cast<uint16_t>(pResultSet->get_unsigned32(3));
        NewWorld.bIsTestWorld = pResultSet->get_boolean(4);
        mmapWorldList[NewWorld.dwWorldId] = NewWorld;
        // Add to packets
        memset(pWorldsAdmin + dwAdminWorlds, 0, sizeof(WORLD_PACKET_ENTRY));
        pWorldsAdmin[dwAdminWorlds].dwWorldID = NewWorld.dwWorldId;
        strncpy(pWorldsAdmin[dwAdminWorlds].szWorldName, NewWorld.szWorldName, sizeof(pWorldsAdmin[dwAdminWorlds].szWorldName) - 1);
        mdwWorldsPacketAdminSize += sizeof(WORLD_PACKET_ENTRY);
        dwAdminWorlds += 1;
        if (!NewWorld.bIsTestWorld) {
            // Not a test world so add to user packet as well
            memset(pWorldsUser + dwUserWorlds, 0, sizeof(WORLD_PACKET_ENTRY));
            pWorldsUser[dwUserWorlds].dwWorldID = NewWorld.dwWorldId;
            strncpy(pWorldsUser[dwUserWorlds].szWorldName, NewWorld.szWorldName, sizeof(pWorldsUser[dwUserWorlds].szWorldName) - 1);
            mdwWorldsPacketUserSize += sizeof(WORLD_PACKET_ENTRY);
            dwUserWorlds += 1;
        }
    }
    // Make sure we have at least one world that users can connect to
    if ((dwAdminWorlds == 0) || (dwUserWorlds == 0)) {
        LOG_CRITICAL("World list is empty or all worlds marked as test.");
        throw std::runtime_error("No user worlds");
    }
    mbWorldListLoaded = true;
}
