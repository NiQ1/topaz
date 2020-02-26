/**
 *	@file WorldManager.h
 *	Manages world list and MQ connections to world servers
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_WORLDMANAGER_H
#define FFXI_LOGIN_WORLDMANAGER_H

#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include <memory>
#include "MQConnection.h"

// Easy way to lock the config mutex
#define LOCK_WORLDMGR std::lock_guard<std::recursive_mutex> l_worldmgr(*WorldManager::GetMutex())

class WorldManager;
typedef WorldManager* WorldManagerPtr;

/**
 *  Singleton class for accessing the world manager.
 */
class WorldManager
{
public:

    /**
     *  Get the name of a world given its ID
     *  @param dwWorldID World ID to look up
     *  @return World name
     */
    std::string GetWorldName(uint32_t dwWorldID);

    /**
     *  Send a message to the MQ server of a world given its ID
     *  @param dwWorldID World ID of the target world
     *  @param bufMessage Message to send
     *  @param cbMessage Message size in bytes
     */
    void SendMessageToWorld(uint32_t dwWorldID, const uint8_t* bufMessage, uint32_t cbMessage);

    /**
     *  Get a precompiled world list packet for administrators and
     *  testers (which contains worlds marked as test).
     *  @return Precompiled packet excluding FFXI headers
     */
    std::shared_ptr<uint8_t> GetAdminWorldsPacket();

    /**
     *  Get the size of the world list packet for administrators.
     */
    uint32_t GetAdminWorldsPacketSize();

    /**
     *  Get a precompiled world list packet for regular users.
     *  This does not include any worlds marked at test.
     *  @return Precompiled packet excluding FFXI headers
     */
    std::shared_ptr<uint8_t> GetUserWorldsPacket();

    /**
     *  Get the size of the world list packet for users.
     */
    uint32_t GetUserWorldsPacketSize();

    /**
     *  Load the world list from the DB
     */
    void LoadWorlds();

    /**
     *  Get an instance of the object. The object is created
     *  on the first call.
     */
    static WorldManagerPtr GetInstance();

    /**
     *  Gets the global data Mutex object. Lock this before
     *  any database access.
     *  @return Database mutex object.
     */
    static std::recursive_mutex* GetMutex();

    /**
     *  Destroy and remove the current instance. Allows reloading of
     *  the configuration file. Should generally be called only before
     *  the server ends execution.
     */
    static void Destroy();

    /**
     *  Destructor. Generally calling Destroy explicitly is much safer.
     */
    ~WorldManager();

    /**
     *  Single world detail record.
     */
    struct WORLD_ENTRY
    {
        uint32_t dwWorldId;
        char szWorldName[16];
        char szMQIP[40];
        uint16_t wMQPort;
        bool bMQUseSSL;
        bool bMQSSLVerifyCA;
        std::shared_ptr<uint8_t> pbufCACert;
        size_t cbCACert;
        std::shared_ptr<uint8_t> pbufClientCert;
        size_t cbClientCert;
        std::shared_ptr<uint8_t> pbufClientKey;
        size_t cbClientKey;
        char szUsername[128];
        char szPassword[128];
        char szVhost[128];
        bool bIsTestWorld;
        std::shared_ptr<MQConnection> pMQConn;
    };

#pragma pack(push, 1)
    /**
     *  World entry as it appears in the FFXI view packet
     */
    struct WORLD_PACKET_ENTRY
    {
        uint32_t dwWorldID;
        char szWorldName[16];
    };
#pragma pack(pop)

private:
    /**
     *  Private constructor
     */
    WorldManager();

    /// World list packet for admins (contains test servers)
    std::shared_ptr<uint8_t> mbufWorldsPacketAdmin;
    /// Size of the admin world list packet
    uint32_t mdwWorldsPacketAdminSize;
    /// World list packet for users (does not contain test servers)
    std::shared_ptr<uint8_t> mbufWorldsPacketUser;
    /// Size of the user world list packet
    uint32_t mdwWorldsPacketUserSize;
    /// List of worlds known to this server
    std::unordered_map<uint32_t, WORLD_ENTRY> mmapWorldList;
    /// Have we already loaded the world list
    bool mbWorldListLoaded = false;
    
    /// Current singleton object
    static WorldManagerPtr smpSingletonObj;
    /// Current object is already being destroyed
    static bool sbBeingDestroyed;
    /// Config access mutex
    std::recursive_mutex mMutex;
};

#endif
