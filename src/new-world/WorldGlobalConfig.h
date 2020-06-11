/**
 *	@file WorldGlobalConfig.h
 *	Reads and stores the global configuration
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_WORLD_WORLDGLOBALCONFIG_H
#define FFXI_WORLD_WORLDGLOBALCONFIG_H

#include "new-common/GlobalConfig.h"

 // Default configuration file name
#define WORLD_DEFAULT_CONFIG_FILE_NAME "world.conf"

/**
 *  Singleton class for reading and accessing configuration.
 */
class WorldGlobalConfig : public GlobalConfig
{
public:

    /**
     *  Get an instance of the configuration. The object is created
     *  on the first call.
     */
    static GlobalConfigPtr GetInstance();

    /**
     *  Get an instance of the configuration. The object is created
     *  on the first call, using the given configuration file name.
     *  On sequent calls this argument is ignored.
     *  @param strConfigFileName Name of the configuration file
     */
    static GlobalConfigPtr GetInstance(const std::string& strConfigFileName);

    /**
     *  Destructor, closes the config file. Generally calling Destroy
     *  explicitly is much safer.
     */
    ~WorldGlobalConfig();

private:

    /**
     *  Read and parse a configuration file.
     *  @param strConfigFileName Name of the configuration file to parse
     */
    WorldGlobalConfig(const std::string& strConfigFileName);

    /**
     *  Default constructor, read and parse the default configuration
     */
    WorldGlobalConfig();

    /**
     *  Get a hardcoded default value for the given configuration value.
     *  @param strConfigName Name of the configuration value to fetch
     *  @return The configuration value content
     */
    virtual std::string GetDefaultValue(const std::string& strConfigName);

};

#endif
