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

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include "Command.h"
#include "ThreadPoolExecutor.h"

extern "C" {
extern char **environ;
}

namespace epics {
namespace execute {

namespace {

/**
 * Provides a pipe together with a thread that reads from this pipe. When the
 * pipe is closed on the writer's side, the thread terminates and provides the
 * result. If the writer writes more data than the capacity, any extra data is
 * simply discarded.
 */
class AccumulatingPipe {

public:

  AccumulatingPipe() : AccumulatingPipe(0) {
  }

  AccumulatingPipe(std::size_t capacity) : capacity(capacity), readFd(-1),
      valid(false), writeFd(-1) {
    if (capacity == 0) {
      // If we are not supposed to read any data, we do not have to create
      // a pipe either.
      this->valid = true;
      return;
    }
    int fileDescriptors[2];
    if (::pipe(fileDescriptors)) {
      throw std::system_error(std::error_code(errno, std::system_category()),
          "pipe() failed");
    }
    this->readFd = fileDescriptors[0];
    this->writeFd = fileDescriptors[1];
    this->valid = true;
  }

  ~AccumulatingPipe() {
    if (this->readFd != -1) {
      ::close(this->readFd);
      this->readFd = -1;
    }
    if (this->writeFd != -1) {
      ::close(this->writeFd);
      this->writeFd = -1;
    }
  }

  std::future<std::vector<char>> readDataAsync() {
    // This method is only called on the read side and only after forking. This
    // means that we can close the write FD. We use some flags in order to
    // ensure that the calling code actually uses this method correctly
    // (does not call both readDataAsync and transferWriteFd or call either of
    // them more than once).
    if (!valid) {
      throw std::logic_error(
          "Only one of readDataAsync and transferWriteFd must be called in each process and each method must only be called once.");
    }
    if (capacity == 0) {
      // If the capacity is zero, we do not have a pipe and can always return an
      // empty vector.
      std::promise<std::vector<char>> promise;
      promise.set_value(std::vector<char>());
      return promise.get_future();
    }
    valid = false;
    ::close(this->writeFd);
    this->writeFd = -1;
    auto future = sharedThreadPoolExecutor().submit(readData, this->capacity,
        this->readFd);
    // The read FD is now owned (and will be closed) by the new thread, so we
    // set it to -1.
    this->readFd = -1;
    return future;
  }

  int transferWriteFd() {
    // This method is only called on the write side and only after forking. This
    // means that we can close the read FD. We use some flags in order to
    // ensure that the calling code actually uses this method correctly
    // (does not call both readDataAsync and transferWriteFd or call either of
    // them more than once).
    if (!valid) {
      throw std::logic_error(
          "Only one of readDataAsync and transferWriteFd must be called in each process and each method must only be called once.");
    }
    if (capacity == 0) {
      throw std::logic_error(
          "Cannot get the write FD because the capacity is zero.");
    }
    valid = false;
    ::close(this->readFd);
    this->readFd = -1;
    // After calling this method, the calling code is responsible for closing
    // the file descriptor, so we set our internal reference to -1.
    int returnValue = this->writeFd;
    this->writeFd = -1;
    return returnValue;
  }

private:

  std::size_t capacity;
  int readFd;
  bool valid = false;
  int writeFd;

  // We do not want to allow copy or move construction and assignment.
  AccumulatingPipe(AccumulatingPipe const&) = delete;
  AccumulatingPipe(AccumulatingPipe &&) = delete;
  AccumulatingPipe &operator=(AccumulatingPipe const&) = delete;
  AccumulatingPipe &operator=(AccumulatingPipe &&) = delete;

  static std::vector<char> readData(std::size_t capacity, int fd) {
    std::vector<char> buffer(capacity, 0);
    std::size_t totalBytesRead = 0;
    ::ssize_t bytesRead;
    while ((bytesRead = ::read(fd, buffer.data() + totalBytesRead,
        buffer.size() - totalBytesRead)) > 0) {
      totalBytesRead += bytesRead;
      if (totalBytesRead == capacity) {
        // Drain the remaining bytes.
        char tempBuffer[1024];
        do {
        } while((bytesRead = read(fd, tempBuffer, sizeof(tempBuffer))) > 0);
        break;
      }
    }
    if (bytesRead == -1) {
      // If there was an error, we throw an exception.
      std::system_error e(std::error_code(errno, std::system_category()),
          "read() failed");
      ::close(fd);
      throw e;
    } else {
      // Otherwise, we simply close the FD because we wrote all data.
      ::close(fd);
      buffer.resize(totalBytesRead);
      return buffer;
    }
  }

};

/**
 * Stores error information from the child process (if any).
 */
struct ChildProcessStatus {

  int errorNumber = 0;
  int execveStatus = 0;

};

/**
 * Provides a pipe together with a thread that read from a buffer and writes
 * into this pipe. Once all data has been written, the thread closes the pipe
 * and terminates.
 */
class PreFilledPipe {

public:

  PreFilledPipe(std::vector<char> const &buffer) : buffer(buffer), readFd(-1),
      valid(false), writeFd(-1) {
    if (buffer.empty()) {
      // If we are not supposed to write any data, we do not have to create
      // a pipe either.
      this->valid = true;
      return;
    }
    int fileDescriptors[2];
    if (::pipe(fileDescriptors)) {
      throw std::system_error(std::error_code(errno, std::system_category()),
          "mmap() failed");
    }
    this->readFd = fileDescriptors[0];
    this->writeFd = fileDescriptors[1];
    this->valid = true;
  }

  ~PreFilledPipe() {
    if (this->readFd != -1) {
      ::close(this->readFd);
      this->readFd = -1;
    }
    if (this->writeFd != -1) {
      ::close(this->writeFd);
      this->writeFd = -1;
    }
  }

  int transferReadFd() {
    // This method is only called on the read side and only after forking. This
    // means that we can close the write FD. We use some flags in order to
    // ensure that the calling code actually uses this method correctly
    // (does not call both transferReadFd and writeDataAsync or call either of
    // them more than once).
    if (!valid) {
      throw std::logic_error(
          "Only one of transferReadFd, writeDataAsync, and "
          "writeDataAsyncAndWaitForPid must be called in each process and each "
          "method must only be called once.");
    }
    if (buffer.empty()) {
      throw std::logic_error(
          "Cannot get the read FD because the buffer is empty.");
    }
    valid = false;
    ::close(this->writeFd);
    this->writeFd = -1;
    // After calling this method, the calling code is responsible for closing
    // the file descriptor, so we set our internal reference to -1.
    int returnValue = this->readFd;
    this->readFd = -1;
    return returnValue;
  }

  std::future<void> writeDataAsync() {
    return writeDataAsyncInternal(false, 0);
  }

  std::future<void> writeDataAsyncAndWaitForPid(pid_t pid) {
    return writeDataAsyncInternal(true, pid);
  }

private:

  std::vector<char> buffer;
  int readFd;
  bool valid = false;
  int writeFd;

  // We do not want to allow copy or move construction and assignment.
  PreFilledPipe(PreFilledPipe const&) = delete;
  PreFilledPipe(PreFilledPipe &&) = delete;
  PreFilledPipe &operator=(PreFilledPipe const&) = delete;
  PreFilledPipe &operator=(PreFilledPipe &&) = delete;

  static void writeData(
      std::vector<char> buffer, int fd, bool waitForPid, pid_t pid) {
    std::size_t totalBytesWritten = 0;
    ::ssize_t bytesWritten;
    while ((bytesWritten = ::write(fd, buffer.data() + totalBytesWritten,
        buffer.size() - totalBytesWritten)) > 0) {
      totalBytesWritten += bytesWritten;
      if (totalBytesWritten == buffer.size()) {
        bytesWritten = 0;
        break;
      }
    }
    // If we are supposed to wait for a child process, we do this now.
    if (waitForPid) {
      ::waitpid(pid, nullptr, 0);
    }
    // If there was an error, we throw an exception.
    if (bytesWritten == -1) {
      // We construct the exception before calling close(), because close()
      // might reset errno.
      std::system_error e(std::error_code(errno, std::system_category()),
          "write() failed");
      ::close(fd);
      throw e;
    } else {
      // Otherwise, we simply close the FD because we wrote all data.
      ::close(fd);
    }
  }

  std::future<void> writeDataAsyncInternal(bool waitForPid, pid_t pid) {
    // This method is only called on the write side and only after forking. This
    // means that we can close the read FD. We use some flags in order to
    // ensure that the calling code actually uses this method correctly
    // (does not call both transferReadFd and writeDataAsync or call either of
    // them more than once).
    if (!valid) {
      throw std::logic_error(
          "Only one of transferReadFd, writeDataAsync, and "
          "writeDataAsyncAndWaitForPid must be called in each process and each "
          "method must only be called once.");
    }
    valid = false;
    if (buffer.empty() and !waitForPid) {
      // If the buffer is empty, we did not create a pipe, so we do not have to
      // close any file descriptors. However, we still have to wait for the
      // child process, if requested.
      if (waitForPid) {
        return sharedThreadPoolExecutor().submit([pid]() {
              ::waitpid(pid, nullptr, 0);
        });
      }
      // If we do not have to wait for a process, we are done and can simply
      // return a future that has already completed.
      std::promise<void> promise;
      promise.set_value();
      return promise.get_future();
    }
    ::close(this->readFd);
    this->readFd = -1;
    auto future = sharedThreadPoolExecutor().submit(writeData,
        std::move(this->buffer), this->writeFd, waitForPid, pid);
    // The write FD is now owned (and will be closed) by the new thread, so we
    // set it to -1.
    this->writeFd = -1;
    return future;
  }

};

/**
 * Ensures that the running flag (or any other boolean passed to the
 * constructor) is switched from false to true during construction and back from
 * true to false during destruction.
 */
struct RunningFlagGuard {

  bool ignore;
  bool &runningFlag;
  std::mutex &mutex;

  RunningFlagGuard(bool &runningFlag, std::mutex &mutex, bool ignore) :
      ignore(ignore), runningFlag(runningFlag), mutex(mutex) {
    if (!this->ignore) {
      std::lock_guard<std::mutex> lock(this->mutex);
      if (this->runningFlag) {
        throw std::invalid_argument(
          "run() has been called before the previous call to run() finished.");
      }
      this->runningFlag = true;
    }
  }

  ~RunningFlagGuard() {
    if (!ignore) {
      std::lock_guard<std::mutex> lock(mutex);
      runningFlag = false;
    }
  }

};

/**
 * Stores a data structure in a memory segment allocated with mmap. This can be
 * used to share a data structure between the parent and the child process. This
 * class does not implement any synchronization mechanism, so synchronization
 * has to be ensured by the user.
 */
template<typename T>
class SharedData {

public:

  SharedData() : data(nullptr) {
    // We us an anonymous memory mapping in order to establish a communication
    // channel between the parent and the child process. As we only write to the
    // memory in the child process and only read from it in the parent process
    // after the child process has terminated, we do not need any further
    // synchronization mechanism.
    void * mappedMemory =
        ::mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (mappedMemory == MAP_FAILED) {
      std::system_error e(std::error_code(errno, std::system_category()),
        "mmap() failed");
    }
    try {
      this->data = new(mappedMemory) T();
    } catch (...) {
      ::munmap(this->data, sizeof(T));
      this->data = nullptr;
      throw;
    }
  }

  ~SharedData() {
    if (this->data) {
      try {
        this->data->~T();
      } catch (...) {
        ::munmap(this->data, sizeof(T));
        this->data = nullptr;
        throw;
      }
      ::munmap(this->data, sizeof(T));
      this->data = nullptr;
    }
  }

  T *operator->() {
    return this->data;
  }

  T const *operator->() const {
    return this->data;
  }

  T &operator*() {
    return *this->data;
  }

  T const &operator*() const {
    return *this->data;
  }

private:

  T *data;

  // We cannot allow copy construction or assignment, so we delete the
  // respective constructor and operator. We could allow move semantics, but we
  // would have to implement them specifically and we do not need them, so we
  // delete that constructor and operator, too.
  SharedData(SharedData const&) = delete;
  SharedData(SharedData &&) = delete;
  SharedData &operator=(SharedData const&) = delete;
  SharedData &operator=(SharedData &&) = delete;

};

/**
 * The argument values are stored in a map. This function creates a vector,
 * using empty strings for missing indices. The strings are copied, so that
 * changes in the map made after calling this function do not affect the
 * vector.
 */
std::vector<std::string> prepareArguments(
    std::map<int, std::string> const &arguments) {
  // We now that arguments is never empty because there should at least be an
  // argument with index 0.
  assert(!arguments.empty());
  // As arguments is not empty, arguments.rbegin() is never arguments.rend().
  int maxIndex = arguments.rbegin()->first;
  // We initialize the vector with empty strings. After that, we overwrite the
  // entries for the indices which are also defined in the original vector.
  std::vector<std::string> argumentsVector(maxIndex + 1, std::string());
  for (auto argEntry = arguments.begin(); argEntry != arguments.end(); ++argEntry) {
    argumentsVector[argEntry->first] = argEntry->second;
  }
  return argumentsVector;
}

/**
 * Merges this process's environment (represented by the envion variable) with
 * the entries from the specified map. The strings are copied, so future changes
 * to the environ variable or to the map do not affect the returned vector.
 */
std::vector<std::string> prepareEnvironment(
    std::map<std::string, std::string> const &envVars) {
  // First, we count the entries in the environ array so that we can create a
  // vector of sufficient size. This is probably more efficient than resizing
  // the vector as we add elements.
  char **nextEnvironEntry = environ;
  int numberOfEnvironEntries = 0;
  while (*nextEnvironEntry) {
    ++numberOfEnvironEntries;
    ++nextEnvironEntry;
  }
  std::vector<std::string> newEnvironment;
  newEnvironment.reserve(numberOfEnvironEntries + envVars.size());
  // The environ variable is an array of pointers to null-terminated strings.
  // The last entry in this array is the null pointer. Each string has the form
  // NAME=VALUE.
  nextEnvironEntry = environ;
  while (*nextEnvironEntry) {
    std::string envString(*nextEnvironEntry);
    auto eqIndex = envString.find_first_of('=');
    // If the entry does not contain an equals sign, it is not a valid
    // environment entry and we ignore it. Otherwise, we extract the name in
    // order to see whether it collides with an entry in the envVars map.
    if (eqIndex != std::string::npos) {
      auto name = envString.substr(0, eqIndex);
      // We only add the entry if the envVars map does not have an entry with
      // the same name.
      if (envVars.find(name) == envVars.end()) {
        newEnvironment.emplace_back(envString);
      }
    }
    ++nextEnvironEntry;
  }
  // Now we add the entries from the envVars map.
  for (auto envEntry = envVars.begin(); envEntry != envVars.end(); ++envEntry) {
    auto name = envEntry->first;
    auto value = envEntry->second;
    std::string envString;
    envString.reserve(name.length() + value.length() + 1);
    envString.append(name);
    envString.append(1, '=');
    envString.append(value);
    newEnvironment.emplace_back(envString);
  }
  return newEnvironment;
}

/**
 * Converts a vector of strings to a vector of C strings. The strings are not
 * copied, so the returned vector is only valid as long as the passed vector and
 * its strings are not modified. This function is intended for use on the return
 * values of prepareArguments and prepareEnvironment, resulting in vectors that
 * encapsulate arrays that can be passed to the execve system call.
 */
std::vector<char const *> viewAsCStrings(
    std::vector<std::string> const &strings) {
  std::vector<char const *> cStrings;
  // The resulting vector has one element more than the original vector. This
  // allows us to add a null pointer as the last element, thus making it
  // possible to the pointer returned by cStrings.data() as a null-terminated
  // array of null-terminated strings.
  cStrings.reserve(strings.size() + 1);
  for (auto str = strings.begin(); str != strings.end(); ++str) {
    cStrings.push_back(str->c_str());
  }
  // Finally, we have to add the nullptr to make the resulting array
  // null-terminated.
  cStrings.push_back(nullptr);
  // The pointers in the returned vector are only valid as long as the strings
  // contained in passed vector are not modified. This is okay because we only
  // use this function in a context where this is ensured.
  return cStrings;
}

} // anonymous namespace

Command::Command(std::string const &commandPath, bool wait) :
    commandPath(commandPath), exitCode(0), running(false), stderrCapacity(0),
    stdoutCapacity(0), wait(wait) {
      // The first argument when executing the program is the path to the
      // executable itself.
      arguments[0] = commandPath;
}

void Command::ensureStdErrCapacity(std::size_t capacity) {
  std::lock_guard<std::mutex> lock(mutex);
  if (capacity != 0 && !this->wait) {
    throw std::invalid_argument(
        "Buffering stderr is only supported if the wait flag is set.");
  }
  this->stderrCapacity = std::max(this->stderrCapacity, capacity);
}

void Command::ensureStdOutCapacity(std::size_t capacity) {
  if (capacity != 0 && !this->wait) {
    throw std::invalid_argument(
        "Buffering stdout is only supported if the wait flag is set.");
  }
  this->stdoutCapacity = std::max(this->stdoutCapacity, capacity);
}

int Command::getExitCode() const {
  std::lock_guard<std::mutex> lock(mutex);
  return this->exitCode;
}

std::vector<char> Command::getStdErrBuffer() const {
  std::lock_guard<std::mutex> lock(mutex);
  return this->stderrBuffer;
}

std::vector<char> Command::getStdOutBuffer() const {
  std::lock_guard<std::mutex> lock(mutex);
  return this->stdoutBuffer;
}

bool Command::isWait() const {
    return this->wait;
}

void Command::run() {
  RunningFlagGuard runningFlagGuard(running, mutex, !wait);
  std::vector<std::string> cmdArgs;
  std::vector<std::string> cmdEnv;
  std::vector<char> stdinBuffer;
  std::size_t stderrCapacity;
  std::size_t stdoutCapacity;
  {
    std::lock_guard<std::mutex> lock(mutex);
    cmdArgs = prepareArguments(this->arguments);
    cmdEnv = prepareEnvironment(this->envVars);
    stdinBuffer = this->stdinBuffer;
    stderrCapacity = this->stderrCapacity;
    stdoutCapacity = this->stdoutCapacity;
  }
  // The pointers stored in the following two vectors are only valid as long as
  // the original vectors exist and have not been changed. This is okay, because
  // we only need them inside this function.
  auto cmdArgsNullTerminated = viewAsCStrings(cmdArgs);
  auto cmdEnvNullTerminated = viewAsCStrings(cmdEnv);
  // Prepare a shared memory region that we can use to get status information
  // from the child. We use this to get information about a problem that happens
  // after forking, but before the command is successfully executed.
  SharedData<ChildProcessStatus> childProcessStatus;
  // We need a pipe for the standard input. If the buffer providing the input is
  // empty, the pipes are not actually created, so we can always create the
  // object.
  PreFilledPipe stdinPipe(stdinBuffer);
  // We need two pipes for the standard output and error output. If the capacity
  // is zero, the pipes are not actually created, so we can always create the
  // objects.
  AccumulatingPipe stderrPipe(stderrCapacity);
  AccumulatingPipe stdoutPipe(stdoutCapacity);
  // The sysconf function is not guaranteed to be async-signal-safe since
  // POSIX.1-2008 (in previous versions this guarantee existed), so we call
  // sysconf before calling fork.
  int maxFd = ::sysconf(_SC_OPEN_MAX);
  if (maxFd == -1) {
    throw std::system_error(
        std::error_code(errno, std::system_category()),
        "sysconf(_SC_OPEN_MAX) failed");
  }
  auto childPid = ::fork();
  if (childPid == 0) {
    // This code runs in the newly created child process.
    // There is no platform-independent way for closing all open file
    // descriptors, so we only close stdin, stdout, and stderr and assume that
    // other file descriptors that are open have been opened with the O_CLOEXEC
    // flag.
    // We deliberately ignore the return value because it is not a problem if
    // the specified file descriptor was not open.
    ::close(STDIN_FILENO);
    ::close(STDOUT_FILENO);
    ::close(STDERR_FILENO);
    // If we do not use a pipe for all of the three standard file descriptors,
    // we want to bind them to /dev/null. This ensures that the respective file
    // descriptor is not accidentally bound to some other file, which could
    // have unintended effects when the executed program tries to use them (e.g.
    // by calling printf).
    // If there is a pipe for stdin, we have to change the file descriptor
    // number so that it is actually used as stdin.
    if (!stdinBuffer.empty()) {
      int stdinFd = stdinPipe.transferReadFd();
      if (stdinFd != STDIN_FILENO) {
        ::dup2(stdinFd, STDIN_FILENO);
        ::close(stdinFd);
      }
    } else {
      int stdinFd = ::open("/dev/null", O_RDONLY);
      if (stdinFd != -1 && stdinFd != STDIN_FILENO) {
        ::dup2(stdinFd, STDIN_FILENO);
        ::close(stdinFd);
      }
    }
    // If there are pipes for stdout and stderr, we have to change the file
    // descriptor numbers so that they are actually used as stdout and stderr.
    if (stdoutCapacity) {
      int stdoutFd = stdoutPipe.transferWriteFd();
      if (stdoutFd != STDOUT_FILENO) {
        ::dup2(stdoutFd, STDOUT_FILENO);
        ::close(stdoutFd);
      }
    } else {
      int stdoutFd = ::open("/dev/null", O_WRONLY);
      if (stdoutFd != -1 && stdoutFd != STDOUT_FILENO) {
        ::dup2(stdoutFd, STDOUT_FILENO);
        ::close(stdoutFd);
      }
    }
    if (stderrCapacity) {
      int stderrFd = stderrPipe.transferWriteFd();
      if (stderrFd != STDERR_FILENO) {
        ::dup2(stderrFd, STDERR_FILENO);
        ::close(stderrFd);
      }
    } else {
      int stderrFd = ::open("/dev/null", O_WRONLY);
      if (stderrFd != -1 && stderrFd != STDERR_FILENO) {
        ::dup2(stderrFd, STDERR_FILENO);
        ::close(stderrFd);
      }
    }
    // We close all unused file descriptors. We do not want the child process to
    // have access to any file descriptors that do not have the close-on-exec
    // flag set. First of all, this might give the child process access to
    // things that it should not be able to access, but more importantly due to
    // this process being multi-threaded, there is no reliable way of always
    // setting the close-on-exec flag, and leaking pipes intended for other
    // child processes into the wrong child process can cause the pipe not to be
    // closed when the child process quits, causing a thread that is trying to
    // read from that pipe to hang longer than necessary.
    for (int fd = 0; fd <= maxFd; ++fd) {
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        continue;
      }
      ::close(fd);
    }
    // The EPICS IOC blocks a few signals. We do not want these signals to be
    // blocked for the child process, so we unblock all signals. If sigfillset
    // fails (which is extremely unlikely), we do not call sigprocmask. If
    // sigprocmask fails, we ignore this and continue with the signal mask from
    // EPICS.
    ::sigset_t signalSet;
    // We cannot use the qualified form for sigfillset, because it is a
    // preprocessor macro on some platforms.
    if (!sigfillset(&signalSet)) {
      ::sigprocmask(SIG_UNBLOCK, &signalSet, nullptr);
    }
    // We also reset all signal handlers to SIG_DFL, because the process may
    // have inherited signals where the handler has been set to SIG_IGN.
    // Again, we ignore any errors.
    struct sigaction signalAction;
    ::memset(&signalAction, 0, sizeof(signalAction));
    signalAction.sa_handler = SIG_DFL;
    for (int signalNumber = 0; signalNumber < NSIG; ++signalNumber) {
      ::sigaction(signalNumber, &signalAction, nullptr);
    }
    // Using a const cast here is not really clean, but it should be okay
    // because usually execve does not return and if it returns, we are not
    // going to use the data structures any longer.
    int execveStatus = ::execve(commandPath.c_str(),
        const_cast<char * const *>(cmdArgsNullTerminated.data()),
        const_cast<char * const *>(cmdEnvNullTerminated.data()));
    // On success, the call to execve does not return. If it returns, there
    // must be a problem. We write the information about the error into the
    // shared data structure. After the child process has terminated, the parent
    // process can read it from there.
    childProcessStatus->execveStatus = execveStatus;
    childProcessStatus->errorNumber = errno;
    // Now we kill the child process.
    ::_exit(-1);
  } else if (childPid == -1) {
    // The call to fork failed and no child process was created.
    // errno may be a preprocessor macro, so we cannot use the qualified form.
    std::system_error e(std::error_code(errno, std::system_category()),
        "fork() failed");
    // If the wait flag is set, we update the exit code to reflect the problem.
    // We do not do this if the wait flag is not set because we would not update
    // it in the regular case either.
    if (wait) {
      // updateResultState takes the mutex, so we must not take it here.
      updateResultState(exitCodeSystemError);
    }
    throw e;
  } else {
    // The call to fork was successful and this code runs in the
    // parent process.
    if (wait) {
      auto stderrFuture = stderrPipe.readDataAsync();
      auto stdoutFuture = stdoutPipe.readDataAsync();
      auto stdinFuture = stdinPipe.writeDataAsync();
      int childStatus;
      if (::waitpid(childPid, &childStatus, 0) == childPid) {
        // If the execve call was successful, the corresponding status code in
        // the shared data structure must be zero. Otherwise, the error number
        // should also have been set and we throw an exception.
        if (childProcessStatus->execveStatus) {
          // updateResultState takes the mutex, so we must not take it here.
          updateResultState(exitCodeSystemError);
          throw std::system_error(
              std::error_code(childProcessStatus->errorNumber,
                  std::system_category()),
              "execve() failed");
        }
        // WIFEXITED and WIFSIGNALED are preprocessor macros.
        if (WIFEXITED(childStatus)) {
          int exitCode = WEXITSTATUS(childStatus);
          // updateResultState takes the mutex, so we must not take it here.
          updateResultState(exitCode, stdoutFuture.get(), stderrFuture.get());
        } else if (WIFSIGNALED(childStatus)) {
          // updateResultState takes the mutex, so we must not take it here.
          updateResultState(exitCodeKilledBySignal, stdoutFuture.get(),
              stderrFuture.get());
        } else {
          // updateResultState takes the mutex, so we must not take it here.
          updateResultState(exitCodeSystemError, stdoutFuture.get(),
              stderrFuture.get());
          throw std::logic_error(
            "waitpid() returned an unexpected child status.");
        }
        // We also check whether the write thread (supplying data to stdin of
        // the command) was successful. Calling the future's get method is
        // sufficient for throwing an exception that may have been thrown in
        // that thread.
        stdinFuture.get();
      } else {
        std::system_error e(std::error_code(errno, std::system_category()),
            "waitpid() failed");
        // updateResultState takes the mutex, so we must not take it here.
        updateResultState(exitCodeSystemError);
        throw e;
      }
    } else {
      // If we do not wait for the command execution to complete, we simply call
      // writeDataAsyncAndWaitForPid() without waiting for the returned future.
      // The writeDataAsyncAndWaitForPid function spawns a background thread
      // that owns the necessary data, so it is safe to destroy stdinPipe when
      // leaving this block, even if the execution has not finished yet.
      // Not that we use writeDataAsyncAndWaitForPid() instead of
      // writeDataAsync() here. As we do not wait in this thread for the child
      // process to terminate, we have to do this in the spawned thread.
      // Otherwise, the child process would never be reaped after terminating
      // and stay as a zombie process until the whole IOC terminates.
      stdinPipe.writeDataAsyncAndWaitForPid(childPid);
    }
  }
}

void Command::setArgument(int index, std::string const &value) {
  if (index <= 0) {
    throw std::invalid_argument(
      "Command argument index must be greater than zero.");
  }
  std::lock_guard<std::mutex> lock(mutex);
  this->arguments[index] = value;
}

void Command::setEnvVar(std::string const &name, std::string const &value) {
  std::lock_guard<std::mutex> lock(mutex);
  this->envVars[name] = value;
}

void Command::setStdInBuffer(std::vector<char> const &buffer) {
  std::lock_guard<std::mutex> lock(mutex);
  this->stdinBuffer = buffer;
}

void Command::updateResultState(int exitCode, std::vector<char> stdoutBuffer,
    std::vector<char> stderrBuffer) {
  // We assume that the calling code did not take the mutex. Obviously, this
  // will cause problems if it already took the mutex, because the mutex is not
  // recursive. However, we only use this method internally, so in general, this
  // assumption should be safe.
  std::lock_guard<std::mutex> lock(mutex);
  this->exitCode = exitCode;
  this->stderrBuffer = stderrBuffer;
  this->stdoutBuffer = stdoutBuffer;
}


} // namespace epics
} // namespace execute
