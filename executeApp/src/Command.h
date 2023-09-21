/*
 * Copyright 2018-2023 aquenos GmbH.
 * Copyright 2018-2023 Karlsruhe Institute of Technology.
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

#ifndef EPICS_EXEC_COMMAND_H
#define EPICS_EXEC_COMMAND_H

#include <cstdint>
#include <forward_list>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace epics {
namespace execute {

/**
 * Command that may be excuted. This object collects the arguments and
 * environment variables that shall be passed to the command. The actual
 * execution of the command can be started by calling the run() method.
 */
class Command {

public:

  /**
   * Exit code used to indicate that the child process was killed by a signal.
   */
  static int const exitCodeKilledBySignal = -1;

  /**
   * Exit code used to indicate that there was a system error. Typically, this
   * means that the fork() system call failed, but it could also be caused by
   * some other system call failing in the parent process.
   */
  static int const exitCodeSystemError = -2;

  /**
   * Creates a command that runs the executable at the specified path. If wait
   * is true, the call to run() will block until the execution has finished and
   * the commands exit code will be made available through the getExitCode()
   * method. If wait is false, run() returns immediately (right after forking
   * the process) and getExitCode() always returns zero.
   */
  Command(std::string const &commandPath, bool wait);

  /**
   * Increases the capacity of the buffer for the standard error output if the
   * new capacity is greater than the current capacity. Otherwise, the capacity
   * is not changed.
   *
   * @throws std::invalid_argument if capacity is greater than zero and this
   * command's wait flag is not set.
   */
  void ensureStdErrCapacity(std::size_t capacity);

  /**
   * Increases the capacity of the buffer for the standard output if the new
   * capacity is greater than the current capacity. Otherwise, the capacity is
   * not changed.
   *
   * @throws std::invalid_argument if capacity is greater than zero and this
   * command's wait flag is not set.
   */
  void ensureStdOutCapacity(std::size_t capacity);

  /**
   * Returns the exit code of the last command's last invocation. If the command
   * has not run yet or this command's wait flag is false, zero is returned.
   *
   * If the forked process does not terminate regularly (is killed by a signal),
   * the exit code is set to exitCodeKilledBySignal. If a system call (e.g.
   * execve() or fork()) fails, the exit code is set to exitCodeSystemError.
   *
   * @return exit code of last run.
   */
  int getExitCode() const;

  /**
   * Returns the buffer containing the output written to the standard error
   * output by the last invocation of the command. If the command has not run
   * yet, this command's wait flag is false, the capacity set for the standard
   * error output is zero, or the command did not write anything to the standard
   * error output, the returned buffer is empty.
   *
   * The buffer's content is limited to the capacity set when calling
   * ensureStdErrCapacity.
   *
   * @return data written to the standard error output by the command.
   */
  std::vector<char> getStdErrBuffer() const;

  /**
   * Returns the buffer containing the output written to the standard output by
   * the last invocation of the command. If the command has not run yet, this
   * command's wait flag is false, the capacity set for the standard output is
   * zero, or the command did not write anything to the standard output, the
   * returned buffer is empty.
   *
   * The buffer's content is limited to the capacity set when calling
   * ensureStdOutCapacity.
   *
   * @return data written to the standard output by the command.
   */
  std::vector<char> getStdOutBuffer() const;

  /**
   * Returns the wait flag. If true, the run() method only returns after the
   * command has completed and the exit code is updated. If false, the run()
   * method returns right after forking and the exit code is never updated.
   *
   * @return true if run blocks, false otherwise.
   */
  bool isWait() const;

  /**
   * Runs this command. This causes a child process to be forked that executes
   * the command. If the wait flag is set on this command, this method waits
   * until the forked process terminates. Otherwise, this method returns
   * immediately after forking.
   *
   * @throw std::system_error if the process cannot be forked or execution of
   *     the command cannot be started (only if the wait flag is set).
   */
  void run();

  /**
   * Sets the value of an argument passed to the executed command.
   *
   * The index is one-based (index zero is always reserved for the name of the
   * executed command). The argument with the greatest index defines the total
   * number of arguments. If arguments with lower indices have not been set, the
   * empty string is used for these indices.
   */
  void setArgument(int index, std::string const &value);

  /**
   * Sets an environment variable passed to the executed command.
   *
   * By default, the environment of the parent process is passed to the command.
   * This method can be used to override the value of individual environment
   * variables or to add additional variables.
   */
  void setEnvVar(std::string const &name, std::string const &value);

  /**
   * Sets the buffer that is used as the source for the input provided to the
   * command. If the buffer is empty, the command will not receive any input and
   * its standard input file descriptor is going to be closed right from the
   * start.
   */
  void setStdInBuffer(std::vector<char> const &buffer);

private:

  std::string commandPath;
  int exitCode;
  std::map<int, std::string> arguments;
  std::map<std::string, std::string> envVars;
  mutable std::mutex mutex;
  bool running;
  std::vector<char> stderrBuffer;
  std::size_t stderrCapacity;
  std::vector<char> stdinBuffer;
  std::vector<char> stdoutBuffer;
  std::size_t stdoutCapacity;
  bool wait;

  // We do not want to allow copy or move construction and assignment.
  Command(Command const&) = delete;
  Command(Command &&) = delete;
  Command &operator=(Command const&) = delete;
  Command &operator=(Command &&) = delete;

  void updateResultState(int exitCode,
      std::vector<char> stdoutBuffer = std::vector<char>(),
      std::vector<char> stderrBuffer = std::vector<char>());

};

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_COMMAND_H
