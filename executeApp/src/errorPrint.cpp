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

#include <cstdarg>
#include <cstdint>
#include <cstdio>

extern "C" {
#include <unistd.h>
}

#include <epicsThread.h>
#include <epicsTime.h>

#include "errorPrint.h"

namespace epics {
namespace execute {

static void errorPrintInternal(const char *format, const char *timeString,
    const char *threadString, std::va_list varArgs) noexcept {
  bool useAnsiSequences = ::isatty(STDERR_FILENO);
  if (useAnsiSequences) {
    // Set format to bold, red.
    std::fprintf(stderr, "\x1b[1;31m");
  }
  if (timeString) {
    std::fprintf(stderr, "%s ", timeString);
  }
  if (threadString) {
    std::fprintf(stderr, "%s ", threadString);
  }
  std::vfprintf(stderr, format, varArgs);
  if (useAnsiSequences) {
    // Reset format
    std::fprintf(stderr, "\x1b[0m");
  }
  std::fprintf(stderr, "\n");
  std::fflush(stderr);
}

void errorPrintf(const char *format, ...) noexcept {
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, nullptr, nullptr, varArgs);
  va_end(varArgs);
}

void errorExtendedPrintf(const char *format, ...) noexcept {
  constexpr int bufferSize = 1024;
  char buffer[bufferSize];
  const char *timeString;
  try {
    ::epicsTime currentTime = ::epicsTime::getCurrent();
    if (currentTime.strftime(buffer, bufferSize, "%Y/%m/%d %H:%M:%S.%06f")) {
      timeString = buffer;
    } else {
      timeString = nullptr;
    }
  } catch (...) {
    timeString = nullptr;
  }
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, timeString, ::epicsThreadGetNameSelf(), varArgs);
  va_end(varArgs);
}

} // namespace execute
} // namespace epics
