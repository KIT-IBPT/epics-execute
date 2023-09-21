/*
 * Copyright 2023 aquenos GmbH.
 * Copyright 2023 Karlsruhe Institute of Technology.
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

#include "ThreadPoolExecutor.h"

namespace epics {
namespace execute {

ThreadPoolExecutor::ThreadPoolExecutor(
  int maximumNumberOfIdleThreads
) : sharedState(std::make_shared<SharedState>()) {
  this->sharedState->idleThreads = 0;
  this->sharedState->maxIdleThreads = maximumNumberOfIdleThreads;
  this->sharedState->shutdown = false;
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
  {
    std::lock_guard<std::mutex> lock(this->sharedState->mutex);
    this->sharedState->shutdown = true;
  }
  this->sharedState->wakeUpCv.notify_all();
}

void ThreadPoolExecutor::processTasks(
  std::shared_ptr<ThreadPoolExecutor::SharedState> sharedState
) {
  // This method is called in each one of the executor threads.
  std::unique_lock<std::mutex> lock(sharedState->mutex);
  while (true) {
    if (sharedState->pendingTasks.empty()) {
      if (sharedState->shutdown) {
        --sharedState->idleThreads;
        break;
      }
      sharedState->wakeUpCv.wait(lock);
      continue;
    }
    auto task = std::move(sharedState->pendingTasks.front());
    sharedState->pendingTasks.pop_front();
    lock.unlock();
    task();
    lock.lock();
    if (sharedState->idleThreads == sharedState->maxIdleThreads) {
      break;
    }
    ++sharedState->idleThreads;
  }
}

namespace {
  ThreadPoolExecutor sharedExecutor(4);
} // anonymous namespace

ThreadPoolExecutor &sharedThreadPoolExecutor() {
  return sharedExecutor;
}

} // namespace execute
} // namespace epics
