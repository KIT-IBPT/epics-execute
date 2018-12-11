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

#ifndef EPICS_EXEC_BASE_DEVICE_SUPPORT_H
#define EPICS_EXEC_BASE_DEVICE_SUPPORT_H

#include <memory>
#include <set>

#include "CommandRegistry.h"
#include "RecordAddress.h"

namespace epics {
namespace execute {

/**
 * Base class for the device support classes for all records.
 */
template<typename RecType>
class BaseDeviceSupport {

public:

  /**
   * Type of the record that is supported by this device support.
   */
  using RecordType = RecType;

  /**
   * Returns true if conversion in the record support routines should be
   * suppressed. This has the effect that the init and process record functions
   * will return 2 instead of 0 on success.
   */
  inline bool isNoConvert() const {
    return noConvert;
  }

  /**
   * Called each time the record is processed. Used for reading (input
   * records) or writing (output records) data from or to the hardware. The
   * default implementation of the processRecord method works asynchronously by
   * calling the {@link #processPrepare()} method and setting the PACT field to
   * one before returning. When it is called again later, PACT is reset to zero
   * and the {@link #processComplete} is called.
   */
  virtual void processRecord() = 0;

protected:

  /**
   * Constructor. The record is stored in an attribute of this class and
   * is used by all methods which want to access record fields.
   */
  BaseDeviceSupport(RecordType *record, RecordAddress const &address,
      bool noConvert = false);

  /**
   * Destructor.
   */
  virtual ~BaseDeviceSupport() {
  }

  /**
   * Returns the command associated with the record.
   */
  inline std::shared_ptr<Command> getCommand() const {
    return command;
  }

  /**
   * Returns a pointer to the structure that holds the actual EPICS record.
   * Always returns a valid pointer.
   */
  inline RecordType *getRecord() const {
    return record;
  }

  /**
   * Returns the address associated with the record.
   */
  inline const RecordAddress &getRecordAddress() const {
    return address;
  }

private:

  // We do not want to allow copy or move construction or assignment.
  BaseDeviceSupport(const BaseDeviceSupport &) = delete;
  BaseDeviceSupport(BaseDeviceSupport &&) = delete;
  BaseDeviceSupport &operator=(const BaseDeviceSupport &) = delete;
  BaseDeviceSupport &operator=(BaseDeviceSupport &&) = delete;

  /**
   * Address specified in the INP our OUT field of the record.
   */
  RecordAddress address;

  /**
   * Pointer to the server connection.
   */
  std::shared_ptr<Command> command;

  /**
   * Indicates whether conversion between the VAL and RVAL field should be
   * skipped.
   */
  bool noConvert;

  /**
   * Record this device support has been instantiated for.
   */
  RecordType *record;

};

template<typename RecordType>
BaseDeviceSupport<RecordType>::BaseDeviceSupport(RecordType *record,
    RecordAddress const &address, bool noConvert) : address(address),
    command(CommandRegistry::getInstance().getCommand(address.getCommandId())),
    noConvert(noConvert), record(record) {
}

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_BASE_DEVICE_SUPPORT_H
