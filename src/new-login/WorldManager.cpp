/**
 *	@file WorldManager.cpp
 *	Manages world list and MQ connections to world servers
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "WorldManager.h"
#include "new-common/Debugging.h"
#include <mariadb++/connection.hpp>
#include "new-common/Database.h"
#include "LoginGlobalConfig.h"
#include "new-common/Utilities.h"
#include "new-common/CommonMessages.h"

WorldManagerPtr WorldManager::smpSingletonObj = NULL;
bool WorldManager::sbBeingDestroyed = false;

WorldManager::WorldManager() : mbWorldListLoaded(false)
{
    LOG_DEBUG0("Called.");
}

WorldManager::~WorldManager()
{
    if (sbBeingDestroyed == false) {
        Destroy();
    }
}

WorldManagerPtr WorldManager::GetInstance()
{
    if (smpSingletonObj == NULL) {
        smpSingletonObj = new WorldManager();
    }
    return smpSingletonObj;
}


std::recursive_mutex* WorldManager::GetMutex()
{
    LOG_DEBUG0("Called.");
    if (smpSingletonObj == NULL) {
        LOG_CRITICAL("Attempted to access config before initialzing.");
        throw std::runtime_error("Config not initialized.");
    }
    return &smpSingletonObj->mMutex;
}

void WorldManager::Destroy()
{
    LOG_DEBUG0("Called.");
    sbBeingDestroyed = true;
    if (smpSingletonObj == NULL) {
        return;
    }
    // Disconnect from all MQ servers
    while (smpSingletonObj->mmapWorldList.size()) {
        std::unordered_map<uint32_t, WORLD_ENTRY>::iterator it = smpSingletonObj->mmapWorldList.begin();
        it->second.pMQConn->Shutdown();
        smpSingletonObj->mmapWorldList.erase(it->first);
    }
    delete smpSingletonObj;
    smpSingletonObj = NULL;
}

std::string WorldManager::GetWorldName(uint32_t dwWorldID)
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    auto WorldEnt = mmapWorldList.find(dwWorldID);
    if (WorldEnt != mmapWorldList.end()) {
        LOG_ERROR("World ID not found in list.");
        throw std::runtime_error("World ID not found.");
    }
    return std::string(WorldEnt->second.szWorldName);
}

uint32_t WorldManager::GetWorldIDByName(const char* szWorldName)
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    for (auto it = mmapWorldList.begin(); it != mmapWorldList.end(); it++) {
        if (strcmp(it->second.szWorldName, szWorldName) == 0) {
            return it->first;
        }
    }
    LOG_ERROR("World name did not match any known world.");
    throw std::runtime_error("World name not found.");
}

bool WorldManager::IsTestWorld(uint32_t dwWorldID)
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    auto pWorldEnt = mmapWorldList.find(dwWorldID);
    if (pWorldEnt != mmapWorldList.end()) {
        LOG_ERROR("World ID not found in list.");
        throw std::runtime_error("World ID not found.");
    }
    return pWorldEnt->second.bIsTestWorld;
}

std::shared_ptr<uint8_t> WorldManager::GetAdminWorldsPacket()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    return mbufWorldsPacketAdmin;
}

uint32_t WorldManager::GetAdminWorldsPacketSize()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    return mdwWorldsPacketAdminSize;
}

std::shared_ptr<uint8_t> WorldManager::GetUserWorldsPacket()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    return mbufWorldsPacketUser;
}

uint32_t WorldManager::GetUserWorldsPacketSize()
{
    if (!mbWorldListLoaded) {
        LOG_INFO("World list not loaded yet, trying to load now.");
        LoadWorlds();
    }
    LOCK_WORLDMGR;
    return mdwWorldsPacketUserSize;
}

void WorldManager::LoadWorlds()
{
    LOG_DEBUG0("Called.");
    if (mbWorldListLoaded) {
        LOG_DEBUG1("World list already loaded, not loading again.");
        return;
    }

    LOCK_WORLDMGR;

    DBConnection DB = Database::GetDatabase();
    GlobalConfigPtr Config = LoginGlobalConfig::GetInstance();
    LOCK_DB;
    LOCK_CONFIG;

    std::string strSqlQueryFmt("SELECT id, name, mq_server_ip, mq_server_port, mq_use_ssl, "
        "mq_ssl_verify_cert, mq_ssl_ca_cert, mq_ssl_client_cert, mq_ssl_client_key, "
        "mq_username, mq_password, mq_vhost, is_test FROM %sworlds WHERE is_active=1;");
    std::string strSqlFinalQuery(FormatString(&strSqlQueryFmt,
        Database::RealEscapeString(Config->GetConfigString("db_prefix")).c_str()));
    mariadb::result_set_ref pResultSet = DB->query(strSqlFinalQuery);
    uint32_t dwNumWorlds = static_cast<uint32_t>(pResultSet->row_count());
    if (dwNumWorlds == 0) {
        LOG_CRITICAL("Failed to query the world list.");
        throw std::runtime_error("world list query failed.");
    }
    WORLD_ENTRY NewWorld = { 0 };
    // Build the world packets as well, since they are static we might as well do it only once
    // Note that packet size is dynamic (according to the number of worlds). The world list packet
    // consists of one unknown 4 byte header and then world list (4 byte world id, 15 byte world
    // name and 1 byte null terminator).
    mbufWorldsPacketAdmin = std::shared_ptr<uint8_t>(new uint8_t[sizeof(WORLD_PACKET_ENTRY) * dwNumWorlds + 4]);
    *reinterpret_cast<uint32_t*>(mbufWorldsPacketAdmin.get()) = 0x20;
    WORLD_PACKET_ENTRY* pWorldsAdmin = reinterpret_cast<WORLD_PACKET_ENTRY*>((mbufWorldsPacketAdmin.get()) + 4);
    mdwWorldsPacketAdminSize = 4;
    // We don't know what will be the final size of this but it's guaranteed not to be longer
    // than the admin packet.
    mbufWorldsPacketUser = std::shared_ptr<uint8_t>(new uint8_t[sizeof(WORLD_PACKET_ENTRY) * dwNumWorlds + 4]);
    *reinterpret_cast<uint32_t*>(mbufWorldsPacketUser.get()) = 0x20;
    WORLD_PACKET_ENTRY* pWorldsUser = reinterpret_cast<WORLD_PACKET_ENTRY*>(mbufWorldsPacketUser.get() + 4);
    mdwWorldsPacketUserSize = 4;
    uint32_t dwAdminWorlds = 0;
    uint32_t dwUserWorlds = 0;
    while (pResultSet->next()) {
        memset(&NewWorld, 0, sizeof(NewWorld));
        NewWorld.dwWorldId = pResultSet->get_unsigned32(0);
        strncpy(NewWorld.szWorldName, pResultSet->get_string(1).c_str(), sizeof(NewWorld.szWorldName) - 1);
        strncpy(NewWorld.szMQIP, pResultSet->get_string(2).c_str(), sizeof(NewWorld.szMQIP) - 1);
        NewWorld.wMQPort = static_cast<uint16_t>(pResultSet->get_unsigned32(3));
        NewWorld.bMQUseSSL = pResultSet->get_boolean(4);
        NewWorld.bMQSSLVerifyCA = pResultSet->get_boolean(5);
        // 100KB for certs is way more than enough
        NewWorld.pbufCACert = IStreamToBuffer((pResultSet->get_blob(6)).get(), 102400, &NewWorld.cbCACert);
        NewWorld.pbufClientCert = IStreamToBuffer((pResultSet->get_blob(7)).get(), 102400, &NewWorld.cbClientCert);
        NewWorld.pbufClientKey = IStreamToBuffer((pResultSet->get_blob(8)).get(), 102400, &NewWorld.cbClientKey);
        strncpy(NewWorld.szUsername, pResultSet->get_string(9).c_str(), sizeof(NewWorld.szUsername) - 1);
        strncpy(NewWorld.szPassword, pResultSet->get_string(10).c_str(), sizeof(NewWorld.szPassword) - 1);
        strncpy(NewWorld.szVhost, pResultSet->get_string(11).c_str(), sizeof(NewWorld.szVhost) - 1);
        NewWorld.bIsTestWorld = pResultSet->get_boolean(12);
        // Attempt to connect to MQ server
        try {
            NewWorld.pMQConn = std::shared_ptr<MQConnection>(new MQConnection(NewWorld.dwWorldId,
                std::string(NewWorld.szMQIP),
                NewWorld.wMQPort,
                std::string(NewWorld.szUsername),
                std::string(NewWorld.szPassword),
                std::string(NewWorld.szVhost),
                std::string(""),
                std::string(LOGIN_MQ_NAME),
                std::string(LOGIN_MQ_NAME),
                NewWorld.bMQUseSSL,
                NewWorld.bMQSSLVerifyCA,
                NewWorld.pbufCACert.get(),
                NewWorld.cbCACert,
                NewWorld.pbufClientCert.get(),
                NewWorld.cbClientCert,
                NewWorld.pbufClientKey.get(),
                NewWorld.cbClientKey));
        }
        catch (std::exception&) {
            LOG_ERROR("Connection to world MQ failed, this world will be disabled.");
            continue;
        }
        NewWorld.pMQConn->StartThread();
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

void WorldManager::SendMessageToWorld(uint32_t dwWorldID, const uint8_t* bufMessage, uint32_t cbMessage)
{
    mmapWorldList[dwWorldID].pMQConn->Send(bufMessage, cbMessage);
}