/**
 *	@file Utilities.cpp
 *	Misc helper functions
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#include "Utilities.h"
#include "Debugging.h"
#include <stdexcept>

std::string FormatStringV(const std::string* pstrFormat, va_list args)
{
    // Temp buffer on stack so we hopefully don't have to allocate on heap
    char szResult[1024];
    // In case we do need to allocate on heap, use this
    char* pszResultEx = NULL;

    LOG_DEBUG0("Called.");
    szResult[sizeof(szResult) - 1] = '\0';
    int iBytesNeeded = vsnprintf(szResult, sizeof(szResult) - 1, pstrFormat->c_str(), args);
    if (iBytesNeeded >= sizeof(szResult)) {
        pszResultEx = new char[iBytesNeeded + 1];
        pszResultEx[iBytesNeeded] = '\0';
        int iResult = vsnprintf(pszResultEx, sizeof(szResult) - 1, pstrFormat->c_str(), args);
        if ((iResult < 0) || (iResult > iBytesNeeded)) {
            // Should never happen....
            free(pszResultEx);
            LOG_ERROR("vsnprintf failed with dynamic allocation.");
            throw std::runtime_error("vsnprintf failed with dynamic allocation.");
        }
        std::string strResult(pszResultEx);
        delete pszResultEx;
        return strResult;
    }
    else if (iBytesNeeded >= 0) {
        return std::string(szResult);
    }
    LOG_ERROR("vsnprintf failed.");
    throw std::runtime_error("vsnprintf failed.");
}

std::string FormatString(const std::string* pstrFormat, ...)
{
    va_list vaArg;

    va_start(vaArg, pstrFormat);
    std::string strResult(FormatStringV(pstrFormat, vaArg));
    va_end(vaArg);
    return strResult;
}
