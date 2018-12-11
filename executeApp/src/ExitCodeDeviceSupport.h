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

#ifndef EPICS_EXEC_EXIT_CODE_DEVICE_SUPPORT_H
#define EPICS_EXEC_EXIT_CODE_DEVICE_SUPPORT_H

#include <stdexcept>
#include <type_traits>

#include "BaseDeviceSupport.h"
#include "RecordValFieldName.h"

namespace epics {
namespace execute {

/**
 * Device support for records reading the exit code.
 *
 * The input record's value is updated with the exit code of the command's last
 * run. Consequently, this device support code only handles record address of
 * type exitCode.
 */
template <typename RecordType, RecordValFieldName ValFieldName>
class ExitCodeDeviceSupport : public BaseDeviceSupport<RecordType> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   *
   * @throws std::invalid_argument if the wait flag of the command associated
   *     with the record is not set.
   */
  ExitCodeDeviceSupport(RecordType *record, RecordAddress const &address)
      : BaseDeviceSupport<RecordType>(record, address) {
    if (!this->getCommand()->isWait()) {
      throw std::invalid_argument(
          "Cannot read the exit code of a command if the wait flag is not set.");
    }
  }

  /**
   * Updates the record's value with the most recent exit code of the underlying
   * command.
   */
  void processRecord() {
    getValueField(this->getRecord()) = this->getCommand()->getExitCode();
  }

private:

  template<RecordValFieldName V = ValFieldName>
  inline static auto getValueField(typename std::enable_if<V == RecordValFieldName::val, RecordType>::type *record)
      -> typename std::add_lvalue_reference<decltype(record->val)>::type {
    return record->val;
  }

  template<RecordValFieldName V = ValFieldName>
  inline static auto getValueField(typename std::enable_if<V == RecordValFieldName::rval, RecordType>::type *record)
      -> typename std::add_lvalue_reference<decltype(record->rval)>::type {
    return record->rval;
  }

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_EXIT_CODE_DEVICE_SUPPORT_H
