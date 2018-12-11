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

#ifndef EPICS_EXEC_STRINGOUT_STDIN_DEVICE_SUPPORT_H
#define EPICS_EXEC_STRINGOUT_STDIN_DEVICE_SUPPORT_H

#include <cstring>
#include <stdexcept>

extern "C" {
#include <stringoutRecord.h>
} // extern "C"

#include "BaseDeviceSupport.h"
#include "RecordValFieldName.h"

namespace epics {
namespace execute {

/**
 * Device support class for the stringout record when it operates in stdin mode.
 *
 * This device support code only handles a record address of type stdin.
 */
class StringoutStdInDeviceSupport : public BaseDeviceSupport<::stringoutRecord> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   */
  StringoutStdInDeviceSupport(::stringoutRecord *record,
      RecordAddress const &address) : BaseDeviceSupport<::stringoutRecord>(
      record, address) {
  }

  /**
   * Writes the record's value to the underlying command. Only the part of the
   * string in the VAL field before the first null byte is used.
   */
  void processRecord() {
    char *cStr = getRecord()->val;
    std::vector<char> buffer(cStr, cStr + std::strlen(cStr));
    getCommand()->setStdInBuffer(buffer);
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_STRINGOUT_STDIN_DEVICE_SUPPORT_H
