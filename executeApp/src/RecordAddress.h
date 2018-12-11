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

#ifndef EPICS_EXEC_RECORD_ADDRESS_H
#define EPICS_EXEC_RECORD_ADDRESS_H

#include<stdexcept>
#include<string>

extern "C" {
#include <dbLink.h>
} // extern "C"

#include "RecordAddressOption.h"
#include "RecordAddressType.h"

namespace epics {
namespace execute {

/**
 * Record address for the execute device support.
 */
class RecordAddress {

public:

  using Option = RecordAddressOption;
  using Type = RecordAddressType;

  /**
   * Creates a record address using the specified parameters.
   *
   * Typically, this constructor is not called directly. Instead, the
   * parse(::DBLINK address, std::set<Type> const &allowedTypes) method is used
   * for parsing the content of a record's link field and generating an address
   * object from it.
   */
  inline RecordAddress(const std::string &commandId, Type type, int argumentIndex,
      std::string const &envVarName, BitMask<Option> options)
      : argumentIndex(argumentIndex), commandId(commandId),
      envVarName(envVarName), options(options), type(type) {
  }

  /**
   * Returns the argument index. This is the position of the argument when being
   * passed to the command. The argument index is always greater than zero, as
   * the index zero is reserved for the name of the command itself.
   *
   * @throws std::invalid_argument if the type of this address is
   *     not Type::argument.
   */
  inline int getArgumentIndex() const {
    if (type != Type::argument) {
      throw std::invalid_argument(
        "The getArgumentIndex method must only be called if the type is argument.");
    }
    return argumentIndex;
  }

  /**
   * Returns the command ID. The command ID connects the various records
   * belonging to a command to the command definition in the IOC's statup
   * configuration file.
   */
  inline std::string const &getCommandId() const {
    return commandId;
  }

  /**
   * Returns the name of the environment variable.
   *
   * @throws std::invalid_argument if the type of this address is
   *     not Type::envVar.
   */
  inline std::string const &getEnvVarName() const {
    if (type != Type::envVar) {
      throw std::invalid_argument(
        "The getEnvVarName method must only be called if the type is envVar.");
    }
    return envVarName;
  }

  /**
   * Returns the options that are present as part of the address.
   */
  inline BitMask<Option> getOptions() const {
    return options;
  }

  /**
   * Returns the type of this address specification. The type defines the role
   * of the record with respect to the command.
   */
  inline Type getType() const {
    return type;
  }

  /**
   * Parses the contents of a record's link field and returns the corresponding
   * address object.
   *
   * This method is called by the record-specific device-support code, passing
   * the set of types allowed by this record.
   */
  static RecordAddress parse(::DBLINK const &addressField,
      BitMask<Type> allowedTypes);

private:

  int argumentIndex;
  std::string commandId;
  std::string envVarName;
  BitMask<Option> options;
  Type type;

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_RECORD_ADDRESS_H
