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

#ifndef EPICS_EXEC_LSI_DEVICE_SUPPORT_H
#define EPICS_EXEC_LSI_DEVICE_SUPPORT_H

#include <algorithm>
#include <cstring>
#include <stdexcept>

extern "C" {
#include <lsiRecord.h>
#include <menuFtype.h>
} // extern "C"

#include "BaseDeviceSupport.h"

namespace epics {
namespace execute {

/**
 * Device support class for the lsi record.
 *
 * This device support code only handles record address of type stderr or
 * stdout.
 */
class LsiDeviceSupport : public BaseDeviceSupport<::lsiRecord> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   *
   * @throws std::invalid_argument if the wait flag of the command associated
   *     with the record is not set.
   */
  LsiDeviceSupport(::lsiRecord *record, RecordAddress const &address)
      : BaseDeviceSupport<::lsiRecord>(record, address) {
    if (!this->getCommand()->isWait()) {
      throw std::invalid_argument(
          "Cannot read the command's output if its wait flag is not set.");
    }
    // We must ensure that enough of the output is buffered.
    switch (this->getRecordAddress().getType()) {
    case RecordAddress::Type::standardError:
      this->getCommand()->ensureStdErrCapacity(record->sizv - 1);
      break;
    case RecordAddress::Type::standardOutput:
      this->getCommand()->ensureStdOutCapacity(record->sizv - 1);
      break;
    default:
      throw std::logic_error("Unexpected address type.");
    }
  }

  /**
   * Reads the record's value from the underlying command. Depending on the
   * setting specified in the record's OUT field, the record's value is either
   * set as an argument or an environment variable for the specified command.
   */
  void processRecord() {
    char *recordBuffer = this->getRecord()->val;
    std::size_t recordBufferLength = this->getRecord()->sizv;
    std::vector<char> data;
    switch (this->getRecordAddress().getType()) {
    case RecordAddress::Type::standardError:
      data = this->getCommand()->getStdErrBuffer();
      break;
    case RecordAddress::Type::standardOutput:
      data = this->getCommand()->getStdOutBuffer();
      break;
    default:
      throw std::logic_error("Unexpected address type.");
    }
    auto dataLength = std::min(recordBufferLength, data.size());
    std::memcpy(recordBuffer, data.data(), dataLength);
    // If we have less data than the target buffer can take, we fill the rest of
    // the buffer with null bytes.
    // We also have to update the LEN field with the actual length of the string
    // (including the terminating null byte).
    if (dataLength < recordBufferLength) {
      std::memset(recordBuffer + dataLength, 0,
          recordBufferLength - dataLength);
      this->getRecord()->len = dataLength + 1;
    } else {
      // We always have to ensure that the resulting string is null terminated, so
      // we set the last byte to zero.
      recordBuffer[recordBufferLength - 1] = 0;
      this->getRecord()->len = dataLength;
    }
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_LSI_DEVICE_SUPPORT_H
