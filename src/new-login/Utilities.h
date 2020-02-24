/**
 *	@file Utilities.h
 *	Misc helper functions
 *	@author Twilight
 *	@copyright 2020, all rights reserved. Licensed under AGPLv3
 */

#ifndef FFXI_LOGIN_UTILITIES_H
#define FFXI_LOGIN_UTILITIES_H

#include <stdarg.h>
#include <string>
#include <istream>
#include <memory>

/**
 *  Format a string in a C-printf fansion given varargs and return
 *  as C++ string.
 *  @param pstrFormat Format string
 *  @param args Arguments as var_arg object
 *  @return Formatted string
 */
std::string FormatStringV(const std::string* pstrFormat, va_list args);

/**
 *  Format a string in a C-printf fansion given varargs and return
 *  as C++ string.
 *  @param pstrFormat Format string
 *  @param ... any further arguments
 *  @return Formatted string
 */
std::string FormatString(const std::string* pstrFormat, ...);

/**
 *  Reads the entire value of an input stream and return it as a buffer
 *  shared pointer.
 *  @param pStream istream to read from
 *  @param dwMax Max size to read in bytes
 *  @param pdwSize If not null receives the size of the data
 *  @return Pointer to the data
 */
std::shared_ptr<uint8_t> IStreamToBuffer(std::istream* pStream, uint32_t dwMax, size_t* pdwSize = NULL);

#endif
