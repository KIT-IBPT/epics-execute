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

#ifndef EPICS_EXEC_THREAD_POOL_EXECUTOR_H
#define EPICS_EXEC_THREAD_POOL_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace epics {
namespace execute {

/**
 * Executor that runs tasks in separate threads, using a thread pool.
 */
class ThreadPoolExecutor {

public:

  /**
   * Creates a thread pool that keeps up to the specified number of idle
   * threads. More threads might still be created on demand.
   */
  ThreadPoolExecutor(int maximumNumberOfIdleThreads);

  /**
   * Destructor. When the destructor is called, no new submissions are allowed.
   * The destructor does not wait for pending tasks to complete. Those tasks
   * are going to complete in the background after the executor has been
   * destructed.
   */
  ~ThreadPoolExecutor();

  /**
   * Submits a task for execution. The submitted task is executed in a thread
   * of its own. If possible, an existing thread is reused. Otherwise, a new
   * thread is created.
   */
  template<class Function, class... Args>
  std::future<
    typename std::result_of<
      typename std::decay<Function>::type(typename std::decay<Args>::type...)
    >::type
  > submit(Function &&f, Args &&... args);

private:

  struct SharedState {
    int idleThreads;
    int maxIdleThreads;
    std::mutex mutex;
    std::list<std::function<void()>> pendingTasks;
    bool shutdown;
    std::condition_variable wakeUpCv;
  };

  std::shared_ptr<SharedState> sharedState;

  static void processTasks(std::shared_ptr<SharedState> sharedState);

};

template<class Function, class... Args>
std::future<
  typename std::result_of<
    typename std::decay<Function>::type(typename std::decay<Args>::type...)
  >::type
> ThreadPoolExecutor::submit(Function &&f, Args &&... args) {
  // We have to use a shared_ptr because packaged_task is not copyable, so the
  // lambda expression below could not be put inside the pendingTasks list if
  // we used packaged_task directly.
  auto task = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
    std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
  );
  auto future = task->get_future();
  auto runFunc = [task]() {(*task)();};
  {
    std::lock_guard<std::mutex> lock(this->sharedState->mutex);
    this->sharedState->pendingTasks.push_back(runFunc);
    if (this->sharedState->idleThreads) {
      // We decrement idleThreads here instead of in the thread that is woken
      // up. If we did it the other way round, this method might run again
      // before the woken-up thread acquires the mutex and we might thus not
      // create a new thread when we actually have to. By decrementing the
      // counter here, we can avoid this situation, because decrementing the
      // counter and waking up a thread happens as an atomic action without
      // releasing the lock on the mutex in between.
      --this->sharedState->idleThreads;
      this->sharedState->wakeUpCv.notify_one();
    } else {
      // We create the thread and detach it right away. The thread does not
      // reference this object, only the shared state, and the latter is
      // referenced through a shared_ptr, so it will stay alive as long as
      // there is a thread.
      std::thread(
        &ThreadPoolExecutor::processTasks, this->sharedState
      ).detach();
    }
  }
  return future;
}

/**
 * Provides a shared executor instance.
 *
 * The returned instance keeps up to four idle threads.
 */
ThreadPoolExecutor &sharedThreadPoolExecutor();

} // namespace execute
} // namespace epics

#endif // EPICS_EXEC_THREAD_POOL_EXECUTOR_H
