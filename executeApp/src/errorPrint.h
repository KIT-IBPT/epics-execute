/*
 * Copyright 2018 aquenos GmbH.
 * Copyright 2018 Karlsruhe Institute of Technology.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * This software has been developed by aquenos GmbH on behalf of the
 * Karlsruhe Institute of Technology's Institute for Beam Physics and
 * Technology.
 *
 * This software contains code originally developed by aquenos GmbH for
 * the s7nodave EPICS device support. aquenos GmbH has relicensed the
 * affected portions of code from the s7nodave EPICS device support
 * (originally licensed under the terms of the GNU GPL) under the terms
 * of the GNU LGPL version 3 or newer.
 */

#ifndef EPICS_EXEC_PRINT_ERROR_H
#define EPICS_EXEC_PRINT_ERROR_H

namespace epics {
namespace execute {

/**
 * Prints an error message. Only the specified message (without any extra
 * information) is printed to stderr. A newline character is automatically
 * appended to the message.
 */
void errorPrintf(const char *format, ...) noexcept;

/**
 * Prints an error message with the current time and the name of the current
 * thread to stderr. A newline character is automatically appended to the
 * message.
 */
void errorExtendedPrintf(const char *format, ...) noexcept;

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_PRINT_ERROR_H
