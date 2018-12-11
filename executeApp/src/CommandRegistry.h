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

#ifndef EPICS_EXEC_COMMAND_REGISTRY_H
#define EPICS_EXEC_COMMAND_REGISTRY_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include "Command.h"

namespace epics {
namespace execute {

/**
 * Registry holding command instance. Commands are created by this registry
 * during initialization (triggered by IOC shell commands) and can then be
 * retrieved for use by different records.
 *
 * This class implements the singleton pattern and the only instance is returned
 * by the {@link #getInstance()} function.
 */
class CommandRegistry {

public:

  /**
   * Returns the only instance of this class.
   */
  inline static CommandRegistry &getInstance() {
    return instance;
  }

  /**
   * Returns the command with the specified ID. If no command with the ID has
   * has been created, a pointer to null is returned.
   */
  std::shared_ptr<Command> getCommand(std::string const &commandId);

  /**
   * Creates a command using the specified ID and options.
   *
   * If wait is true, the command's run() method will block until the execution
   * has finished and the commands exit code will be made available through its
   * getExitCode() method. If wait is false, run() returns immediately (right
   * after forking the process) and getExitCode() always returns zero.
   *
   * @throws std::invalid_argument if the ID is already in use.
   */
  void createCommand(const std::string &commandId,
      std::string const &commandPath, bool wait);

private:

  // We do not want to allow copy or move construction or assignment.
  CommandRegistry(const CommandRegistry &) = delete;
  CommandRegistry(CommandRegistry &&) = delete;
  CommandRegistry &operator=(const CommandRegistry &) = delete;
  CommandRegistry &operator=(CommandRegistry &&) = delete;

  static CommandRegistry instance;

  std::unordered_map<std::string, std::shared_ptr<Command>> commands;
  std::recursive_mutex mutex;

  CommandRegistry();

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_COMMAND_REGISTRY_H
