/**
 *	@file GlobalData.h
 *	Global data structures and objects
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_GLOBALDATA_H
#define FFXI_LOGIN_GLOBALDATA_H

#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include <memory>

 // Easy way to lock the config mutex
#define LOCK_GLOBDATA std::lock_guard<std::recursive_mutex> l_globdata(*GlobalData::GetMutex())

class GlobalData;
typedef GlobalData* GlobalDataPtr;

/**
 *  Singleton class for accessing global data.
 */
class GlobalData
{
public:

    /**
     *  Get the name of a world given its ID
     *  @param dwWorldID World ID to look up
     *  @return World name
     */
    std::string GetWorldName(uint32_t dwWorldID);

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
     *  Get an instance of the object. The object is created
     *  on the first call.
     */
    static GlobalDataPtr GetInstance();

    /**
     *  Gets the global database Mutex object. Lock this before
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
     *  Destructor, closes the config file. Generally calling Destroy
     *  explicitly is much safer.
     */
    ~GlobalData();

    /**
     *  Single world detail record.
     */
    struct WORLD_ENTRY
    {
        uint32_t dwWorldId;
        char szWorldName[16];
        char szMQIP[40];
        uint16_t wMQPort;
        bool bIsTestWorld;
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
    GlobalData();

    /**
     *  Load the world list from the DB
     */
    void LoadWorlds();
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
    static GlobalDataPtr smpSingletonObj;
    /// Current object is already being destroyed
    static bool sbBeingDestroyed;
    /// Config access mutex
    std::recursive_mutex mMutex;
};

#endif
