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

#endif
