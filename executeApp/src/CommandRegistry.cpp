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

#include "CommandRegistry.h"

namespace epics {
namespace execute {

std::shared_ptr<Command> CommandRegistry::getCommand(
    std::string const &commandId) {
  // We have to hold the mutex in order to protect the map from concurrent
  // access.
  std::lock_guard<std::recursive_mutex> lock(mutex);
  auto command = commands.find(commandId);
  if (command == commands.end()) {
    return std::shared_ptr<Command>();
  } else {
    return command->second;
  }
}

void CommandRegistry::createCommand(const std::string &commandId,
      std::string const &commandPath, bool wait) {
  // We have to hold the mutex in order to protect the map from concurrent
  // access.
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if (commands.count(commandId)) {
    throw std::runtime_error("Command ID is already in use.");
  }
  std::shared_ptr<Command> command =
      std::make_shared<Command>(commandPath, wait);
  commands.insert(std::make_pair(commandId, command));
}

CommandRegistry CommandRegistry::instance;

CommandRegistry::CommandRegistry() {
}

} // namespace epics
} // namespace execute
