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

#ifndef EPICS_EXEC_RUN_DEVICE_SUPPORT_H
#define EPICS_EXEC_RUN_DEVICE_SUPPORT_H

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

extern "C" {
#include <alarm.h>
#include <callback.h>
#include <recGbl.h>
} // extern "C"

#include "BaseDeviceSupport.h"

namespace epics {
namespace execute {

/**
 * Device support for records triggering the command to be run.
 *
 * This device support ignores the record's value and always run the command.
 *
 * If the command's wait flag is set, the record is processed asynchronously
 * and only finishes processing after the process created for the command has
 * terminated.
 *
 * If the command's wait flag is not set, the record is processed synchronously
 * and finishes after the process has been forked.
 */
template <typename RecordType>
class RunDeviceSupport : public BaseDeviceSupport<RecordType> {

public:

  /**
   * Constructor. The parameters are passed to the parent constructor.
   *
   * This constructor resets the record's UDF field and alarm severity and
   * status so that the record does not have an INVALID_ALARM, just because the
   * command has not been run yet.
   *
   * @throws std::invalid_argument if the wait flag is set in the record
   *     address, but not on the associated command.
   */
  RunDeviceSupport(RecordType *record, RecordAddress const &address)
      : BaseDeviceSupport<RecordType>(record, address),
      suspendProcessingUntilCommandTerminated(address.getOptions()
          & RecordAddressOption::wait) {
    if (this->suspendProcessingUntilCommandTerminated
        && !this->getCommand()->isWait()) {
      throw std::invalid_argument(
          "The wait option cannot be specified if the command's wait flag is not set.");
    }
    record->udf = 0;
    recGblResetAlarms(record);
  }

  /**
   * Triggers a run of the command.
   *
   * This method always returns quickly (without blocking). If the command's
   * wait flag is set, the record's PACT field is set to 1 and the record is
   * scheduled to be processed again when the command's execution has finished.
   * If the wait flag is not set, PACT is not set and record processing
   * completes immediately.
   *
   * If the wait flag is set, this method throws an exception if the process
   * cannot be forked or if the child process is killed by a signal. If the wait
   * flag is not set, this method only throws an exception if the process cannot
   * be forked.
   */
  void processRecord() {
    RecordType *record = this->getRecord();
    // If this method is called again because a command terminated, we simply
    // want to complete the processing.
    if (asyncExecutionFuture.valid()) {
      if (asyncExecutionFuture.wait_for(std::chrono::duration<int>::zero())
          != std::future_status::ready) {
        // If the future is not ready yet, this means that the record has been
        // processed again before the command finished. This can only happen if
        // we did not set PACT to 1 (setting PACT ensures that record processing
        // can only be triggered by us). We simple restore the value that
        // indicates that the command is running and return.
        record->val = 1;
        record->rval = 1;
        return;
      }
      record->pact = 0;
      record->val = 0;
      record->rval = 0;
      // The get method throws an exception if the command's run method threw an
      // exception.
      try {
        asyncExecutionFuture.get();
      } catch (...) {
        ::recGblSetSevr(record, WRITE_ALARM, MAJOR_ALARM);
        throw;
      }
      return;
    }
    auto command = this->getCommand();
    if (command->isWait()) {
      asyncExecutionFuture = std::async(std::launch::async, [this]() {
        try {
          this->getCommand()->run();
        } catch (...) {
          // We have to schedule another processing of the record, even if the
          // run method threw an exception.
          ::callbackRequestProcessCallback(&this->processCallback,
              priorityMedium, this->getRecord());
          throw;
        }
        ::callbackRequestProcessCallback(&this->processCallback, priorityMedium,
            this->getRecord());
      });
      if (suspendProcessingUntilCommandTerminated) {
        record->pact = 1;
      }
      record->val = 1;
      record->rval = 1;
    } else {
      record->val = 0;
      record->rval = 0;
      try {
        command->run();
      } catch (...) {
        ::recGblSetSevr(record, WRITE_ALARM, MAJOR_ALARM);
        throw;
      }
    }
  }

private:

  std::future<void> asyncExecutionFuture;
  ::CALLBACK processCallback;
  bool suspendProcessingUntilCommandTerminated;

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_RUN_DEVICE_SUPPORT_H
