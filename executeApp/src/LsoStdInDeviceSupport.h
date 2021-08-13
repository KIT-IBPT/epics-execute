/*
 * Copyright 2018-2021 aquenos GmbH.
 * Copyright 2018-2021 Karlsruhe Institute of Technology.
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

#ifndef EPICS_EXEC_LSO_STDIN_DEVICE_SUPPORT_H
#define EPICS_EXEC_LSO_STDIN_DEVICE_SUPPORT_H

#include <algorithm>
#include <cstdint>
#include <stdexcept>

extern "C" {
#include <string.h>

#include <lsoRecord.h>
#include <menuFtype.h>
} // extern "C"

#include "BaseDeviceSupport.h"
#include "RecordValFieldName.h"

namespace epics {
namespace execute {

/**
 * Device support class for the lso record when it operates in stdin mode.
 *
 * This device support code only handles a record address of type stdin.
 */
class LsoStdInDeviceSupport : public BaseDeviceSupport<::lsoRecord> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   */
  LsoStdInDeviceSupport(::lsoRecord *record,
      RecordAddress const &address) : BaseDeviceSupport<::lsoRecord>(record,
      address) {
  }

  /**
   * Writes the record's value to the underlying command. Only the number of
   * bytes specified by the record's LEN field are used.
   */
  void processRecord() {
    char *buffer = getRecord()->val;
    std::size_t bufferLength = getRecord()->len;
    // The string stored inside the record's buffer should always be
    // null-terminated, but we use std::find instead of std::strlen to be extra
    // safe.
    bufferLength = std::find(buffer, buffer + bufferLength, 0) - buffer;
    getCommand()->setStdInBuffer(
        std::vector<char>(buffer, buffer + bufferLength));
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_LSO_STDIN_DEVICE_SUPPORT_H
