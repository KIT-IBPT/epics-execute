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

#ifndef EPICS_EXEC_AAO_OUTPUT_PARAMETER_DEVICE_SUPPORT_H
#define EPICS_EXEC_AAO_OUTPUT_PARAMETER_DEVICE_SUPPORT_H

#include <algorithm>
#include <stdexcept>

extern "C" {
#include <aaoRecord.h>
#include <menuFtype.h>
} // extern "C"

#include "BaseDeviceSupport.h"

namespace epics {
namespace execute {

/**
 * Device support class for the aao record when operating in argument or envVar
 * mode.
 *
 * This device support code only handles record addresses of type argument or
 * envVar.
 */
class AaoOutputParameterDeviceSupport : public BaseDeviceSupport<::aaoRecord> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   *
   * @throws std::invalid_argument if the record's FTVL field is not set to CHAR
   *     or UCHAR.
   */
  AaoOutputParameterDeviceSupport(::aaoRecord *record,
      RecordAddress const &address) : BaseDeviceSupport<::aaoRecord>(record,
      address) {
    if (record->ftvl != menuFtypeCHAR && record->ftvl != menuFtypeUCHAR) {
      throw std::invalid_argument(
          "The record's FTVL field must be set to CHAR or UCHAR.");
    }
  }

  /**
   * Writes the record's value to the underlying command. Depending on the
   * setting specified in the record's OUT field, the record's value is either
   * set as an argument or an environment variable for the specified command.
   */
  void processRecord() {
    auto str = static_cast<char *>(getRecord()->bptr);
    auto maxStrLength = getRecord()->nelm;
    // The string stored inside the record's buffer might not be
    // null-terminated, so we cannot use std::strlen.
    auto strLength = std::find(str, str + maxStrLength, 0) - str;
    auto strValue = std::string(str, strLength);
    RecordAddress const &recordAddress = this->getRecordAddress();
    switch (recordAddress.getType()) {
    case RecordAddress::Type::argument:
      this->getCommand()->setArgument(recordAddress.getArgumentIndex(),
          strValue);
      break;
    case RecordAddress::Type::envVar:
      this->getCommand()->setEnvVar(recordAddress.getEnvVarName(),
          strValue);
      break;
    default:
      throw std::logic_error("Unexpected address type.");
      break;
    }
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_AAO_OUTPUT_PARAMETER_DEVICE_SUPPORT_H
