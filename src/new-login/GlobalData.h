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

 // Easy way to lock the config mutex
#define LOCK_GLOBDATA std::lock_guard<std::mutex> l_globdata(*GlobalData::GetMutex())

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
     *  Get an instance of the object. The object is created
     *  on the first call.
     */
    static GlobalDataPtr GetInstance();

    /**
     *  Gets the global database Mutex object. Lock this before
     *  any database access.
     *  @return Database mutex object.
     */
    static std::mutex* GetMutex();

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
    };

private:
    /**
     *  Private constructor
     */
    GlobalData();

    /**
     *  Load the world list from the DB
     */
    void LoadWorlds();

    /// List of worlds known to this server
    std::unordered_map<uint32_t, WORLD_ENTRY> mmapWorldList;
    /// Have we already loaded the world list
    bool mbWorldListLoaded = false;

    /// Current singleton object
    static GlobalDataPtr smpSingletonObj;
    /// Current object is already being destroyed
    static bool sbBeingDestroyed;
    /// Config access mutex
    std::mutex mMutex;
};

#endif
