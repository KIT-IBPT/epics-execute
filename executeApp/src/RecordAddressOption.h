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

#ifndef EPICS_EXEC_RECORD_ADDRESS_OPTION_H
#define EPICS_EXEC_RECORD_ADDRESS_OPTION_H

#include "BitMask.h"

namespace epics {
namespace execute {

/**
 * Option that can be used in a record address. The set of options that are
 * supported in a record address can depend on both the record type and the
 * specified address type.
 */
enum class RecordAddressOption {

  /**
   * Wait until the executed command has terminated before completing
   * processing of the record. This option may only be used in combination
   * with the run type. It is only allowed if the command's wait flag is set
   * as well.
   */
  wait = 1

};

/**
 * Enable bit-mask operators for the option enum.
 */
template<>
struct EnableBitMask<RecordAddressOption> : public std::true_type {
};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_RECORD_ADDRESS_OPTION_H
