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

#ifndef EPICS_EXEC_AAO_STDIN_DEVICE_SUPPORT_H
#define EPICS_EXEC_AAO_STDIN_DEVICE_SUPPORT_H

#include <algorithm>
#include <cstdint>
#include <stdexcept>

extern "C" {
#include <aaoRecord.h>
#include <menuFtype.h>
} // extern "C"

#include "BaseDeviceSupport.h"
#include "RecordValFieldName.h"

namespace epics {
namespace execute {

/**
 * Device support class for the aao record when it operates in stdin mode.
 *
 * This device support code only handles a record address of type stdin.
 */
class AaoStdInDeviceSupport : public BaseDeviceSupport<::aaoRecord> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   *
   * @throws std::invalid_argument if the record's FTVL field is not set to CHAR
   *     or UCHAR.
   */
  AaoStdInDeviceSupport(::aaoRecord *record,
      RecordAddress const &address) : BaseDeviceSupport<::aaoRecord>(record,
      address) {
    if (record->ftvl != menuFtypeCHAR && record->ftvl != menuFtypeUCHAR) {
      throw std::invalid_argument(
          "The record's FTVL field must be set to CHAR or UCHAR.");
    }
  }

  /**
   * Writes the record's value to the underlying command. Only the number of
   * bytes specified by the record's NORD field are used.
   */
  void processRecord() {
    char *buffer = static_cast<char *>(getRecord()->bptr);
    std::size_t bufferLength = getRecord()->nord;
    getCommand()->setStdInBuffer(
        std::vector<char>(buffer, buffer + bufferLength));
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_AAO_STDIN_DEVICE_SUPPORT_H
