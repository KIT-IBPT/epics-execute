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

#include <regex>
#include <stdexcept>
#include <string>
#include <cstring>

extern "C" {
#include <epicsExport.h>
#include <iocsh.h>
} // extern "C"

#include "CommandRegistry.h"
#include "errorPrint.h"

using namespace epics::execute;

extern "C" {

// Data structures needed for the iocsh executeAddCommand function.
static const iocshArg iocshExecuteAddCommandArg0 = { "command ID",
    iocshArgString };
static const iocshArg iocshExecuteAddCommandArg1= { "command path",
    iocshArgString };
static const iocshArg iocshExecuteAddCommandArg2= { "do not wait",
    iocshArgInt };
static const iocshArg * const iocshExecuteAddCommandArgs[] = {
    &iocshExecuteAddCommandArg0, &iocshExecuteAddCommandArg1,
    &iocshExecuteAddCommandArg2};
static const iocshFuncDef iocshExecuteAddCommandFuncDef = {
    "executeAddCommand", 3, iocshExecuteAddCommandArgs };

static void iocshExecuteAddCommandFunc(const iocshArgBuf *args) noexcept {
  char *commandIdCStr = args[0].sval;
  char *commandPathCStr = args[1].sval;
  int doNotWait = args[2].ival;
  // Verify that the required parameters are set.
  if (!commandIdCStr || !std::strlen(commandIdCStr)) {
    errorPrintf(
        "Could not add the command: Command ID must be specified.");
    return;
  }
  if (!commandPathCStr || !std::strlen(commandPathCStr)) {
    errorPrintf(
        "Could not add the command: Command path must be specified.");
    return;
  }
  auto commandId = std::string(commandIdCStr);
  auto commandPath = std::string(commandPathCStr);
  bool waitFlag = !doNotWait;
  // Verify that the command ID only contains valid characters.
  if (!std::regex_match(commandId, std::regex("[A-Za-z0-9_]+"))) {
    errorPrintf(
        "Could not add the command: Command ID contains invalid characters.");
    return;
  }
  try {
    CommandRegistry::getInstance().createCommand(commandId, commandPath,
       waitFlag);
  } catch (std::exception &e) {
    errorPrintf(
        "Could not add the command: %s", e.what());
  } catch (...) {
    errorPrintf(
        "Could not add the command: Unknown error.");
  }
}

/**
 * Registrar that registers the iocsh commands.
 */
static void executeRegistrar() {
  ::iocshRegister(&iocshExecuteAddCommandFuncDef, iocshExecuteAddCommandFunc);
}

epicsExportRegistrar(executeRegistrar);

} // extern "C"
