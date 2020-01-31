/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/parallel.h"
#include "skip/parallel-extc.h"

#include "skip/Exception.h"
#include "skip/Obstack.h"
#include "skip/Process.h"
#include "skip/String.h"
#include "skip/System.h"
#include "skip/external.h"
#include "skip/memoize.h"
#include "skip/objects.h"

#include "ObstackDetail.h"

#include <boost/intrusive_ptr.hpp>

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <utility>

#ifdef __linux__
#include <sched.h>
#include <sys/sysinfo.h>
#endif

// Returns the number of CPUs available to this process, which may be
// fewer than the number of CPUs present on this machine.
size_t skip::computeCpuCount() {
  if (auto env = std::getenv("SKIP_NUM_THREADS")) {
    std::stringstream sstream(env);
    size_t skip_num_threads = 0;
    sstream >> skip_num_threads;
    return std::max((size_t)1, skip_num_threads);
  }

#ifdef __linux__
  // std::thread::hardware_concurrency() ignores the affinity set by
  // sched_setaffinity() or taskset, so don't use it. Instead, check the
  // the current affinity set to see how many CPUs this process actually
  // has access to.
  //
  // Also, use a dynamically sized CPU set to handle machines where
  // cpu_set_t is too small.
  for (int maxCPUs = 2048; maxCPUs <= 1024 * 1024; maxCPUs *= 2) {
    auto cpus = CPU_ALLOC(maxCPUs);
    if (cpus == nullptr) {
      throw std::bad_alloc();
    }

    auto size = CPU_ALLOC_SIZE(maxCPUs);
    CPU_ZERO_S(size, cpus);

    if (sched_getaffinity(0, size, cpus) == 0) {
      auto count = CPU_COUNT_S(size, cpus);
      CPU_FREE(cpus);
      return (size_t)count;
    }

    auto err = errno;
    CPU_FREE(cpus);

    if (err != EINVAL) {
      errno = err;
      errnoFatal("sched_getaffinity() failed");
    }
  }
#endif

  // If the host doesn't know the count, arbitrarily guess 8.
  auto count = std::thread::hardware_concurrency();
  return count ? (size_t)count : 8;
}

namespace {

using namespace skip;

// Number of available CPUs.
size_t s_numThreads = 1;

class ThreadPool {
 public:
  ThreadPool(size_t n) : m_shutdown{false}, m_exn{nullptr} {
    m_threads.reserve(n);
    for (auto i = 0; i < n; ++i)
      m_threads.emplace_back(std::bind(&ThreadPool::run, this, i));
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> l(m_threadLock);
      m_shutdown = true;
      m_threadVar.notify_all();
    }

    for (auto& thread : m_threads) {
      thread.join();
    }
  }

  void addTask(std::function<void(void)> f) {
    std::unique_lock<std::mutex> l(m_threadLock);
    m_tasks.emplace(std::move(f));
    m_threadVar.notify_one();
  }

  std::exception_ptr waitForThreadsAndThrowIfNecessary() {
    std::unique_lock<std::mutex> lock(m_masterLock);
    m_masterVar.wait(lock);
    if (m_exn != nullptr) {
      std::rethrow_exception(m_exn);
    }
    return m_exn;
  }

 private:
  void run(int i) {
    skip::initializeThreadWithPermanentProcess();
    std::function<void(void)> task;
    auto isFinished = false;

    while (1) {
      {
        std::unique_lock<std::mutex> l(m_threadLock);

        while (!m_shutdown && m_tasks.empty()) {
          m_threadVar.wait(l);
        }

        if (m_shutdown)
          return;

        task = std::move(m_tasks.front());
        m_tasks.pop();
        if (m_tasks.empty()) {
          isFinished = true;
        }
      }

      try {
        task();
      } catch (SkipException& exc) {
        std::lock_guard<std::mutex> lock{m_threadLock};
        m_exn = make_exception_ptr(exc);
      }

      if (isFinished) {
        std::unique_lock<std::mutex> lock(m_masterLock);
        m_masterVar.notify_one();
      }
    }
  }

  bool m_firstCall;
  bool m_shutdown;
  std::exception_ptr m_exn;
  std::mutex m_threadLock;
  std::mutex m_masterLock;
  std::condition_variable m_threadVar;
  std::condition_variable m_masterVar;
  std::queue<std::function<void(void)>> m_tasks;
  std::vector<std::thread> m_threads;
};

ThreadPool* getWorkers() {
  static ThreadPool workers(s_numThreads);
  return &workers;
}

struct Tabulate;

struct TabulateWorker : private skip::noncopyable {
  explicit TabulateWorker(Tabulate* master);

  // Processes all available array entries.
  void run();

  // Compute one array entry. Returns nullptr on exception.
  AObjBase* computeOne(int64_t index);

  Tabulate* const m_master;

  // If there was an exception, what was it, and at what index?
  bool m_hasException = false;
  RObj* m_exception = nullptr;
  int m_exitExceptionStatus = 0;
  int64_t m_exceptionIndex = 0;

  // Deferred results not yet copied to the master Obstack.
  std::vector<std::pair<RObj*, int64_t>> m_results;

  Process::Ptr m_process;

 private:
  void
  finishWithException(int64_t index, RObj* skipException, int exitException);
};

// This manages a single Parallel.tabulate call, which calls a given
// user function once for each integer in the range [0, count), and records
// the results in an Array.
//
// It posts tasks requesting worker threads help process its input. It acts
// as a worker thread itself on this problem, so even if no worker threads
// are available it will still make progress.
//
// Rather than posting a separate task for each "count" value to be computed,
// it posts no more than one per core, and each worker uses an atomic<size_t>
// to find individual "count" values that haven't been worked on yet. So
// worker threads work on this until there are no more values for them to
// compute.
struct Tabulate : private skip::noncopyable {
  using Ptr = boost::intrusive_ptr<Tabulate>;

  Tabulate(int64_t count, RObj* closure)
      : m_index(0),
        m_numActiveThreads(-1),
        m_refcount(1),
        m_count(count),
        m_closure(closure),
        m_ctx(Context::current()),
        m_masterProcess(UnownedProcess(Process::cur())),
        m_resultNote(Obstack::cur().note()) {
    // This implementation is incapable of returning an empty Array,
    // because it needs at least one result to learn the Array's vtable.
    // The caller ensures this is not called in that case.
    assert(count != 0);
  }

  RObj* runMasterThread() {
    spawnWorkers();
    runWorkerThread();
    waitForWorkers();

    // Clear this reference so we don't artifically keep the current Process
    // alive. This matters because this Tabulate object will live on as long
    // as there are any pending tasks posted trying to summon worker threads.
    // Since we've already finished, those tasks will end up doing nothing
    // as soon as they get here, and don't need this field to still be set.
    m_masterProcess = UnownedProcess();

    checkForException();

    return m_results;
  }

  // Called in the master Process. Extracts data from the given worker
  // and assumes responsibility for freeing it.
  void retireWorker(TabulateWorker* wp) {
    std::unique_ptr<TabulateWorker> w{wp};

    m_numActiveThreads.fetch_sub(1, std::memory_order_relaxed);

    // TODO: Atomically steal all Handles, changing their ownership
    // to the master Process, then steal all Strand work queue entries.

    if (LIKELY(!w->m_hasException)) {
      // Don't bother copying back results if we encountered an exception.
      if (!m_thrower) {
        copyResults(*w);
      }
    } else if (
        !m_thrower || w->m_exceptionIndex < m_thrower->m_exceptionIndex) {
      // Remember the exception from the lowest-numbered array index.
      m_thrower = std::move(w);
    }
  }

  // Atomically hand out the next index no one is working on yet.
  // If >= m_count, that means no work remains.
  int64_t nextIndex() {
    return m_index.fetch_add(1, std::memory_order_relaxed);
  }

  // Invoke the user's closure for the given index. This returns
  // a one-entry Array that boxes m_closure's return value.
  AObjBase* callClosure(int64_t index) {
    auto code = reinterpret_cast<AObjBase* (*)(RObj*, int64_t)>(
        m_closure->vtable().getFunctionPtr());
    return code(m_closure, index);
  }

  // Entry point for worker threads.
  void runWorkerThread() {
    if (allWorkTaken()) {
      return;
    }

    auto numActive = m_numActiveThreads.load(std::memory_order_relaxed);
    do {
      if (numActive == 0) {
        // We are late to the party -- other threads have already come and
        // gone, finishing all the work. So bail out.
        return;
      }
    } while (!m_numActiveThreads.compare_exchange_weak(
        numActive, (numActive == (size_t)-1) ? 1 : numActive + 1));

    auto w = new TabulateWorker(this);
    w->run();
    m_masterProcess.schedule([this, w]() { retireWorker(w); });
  }

 private:
  friend void intrusive_ptr_add_ref(Tabulate*);
  friend void intrusive_ptr_release(Tabulate*);

  // Allocate the final results Array, copying the vtable from sampleArray.
  void allocResults(RObj& sampleArray) {
    const auto arraySize = static_cast<arraysize_t>(m_count);
    auto vtable = sampleArray.vtable();
    auto& type = vtable.type();
    m_slotSize = type.userByteSize();
    auto userSize = arraySize * m_slotSize;
    auto metaSize = type.uninternedMetadataByteSize();
    auto size = metaSize + userSize;
    auto roundedSize = roundUp(size, Obstack::kAllocAlign);

    auto& obstack = Obstack::cur();
    auto data = obstack.alloc(roundedSize);

    // Guarantee that any trailing padding cruft at the end is zeroed out,
    // without bothering to zero the entire thing.
    memset(
        mem::add(data, roundedSize - Obstack::kAllocAlign),
        0,
        Obstack::kAllocAlign);

    // Initialize the header.
    m_results = static_cast<AObjBase*>(mem::add(data, metaSize));
    m_results->vtable() = vtable;
    m_results->setArraySize(arraySize);

    // Remember how many pointers are in each slot of the final Array.
    size_t nPtr = 0;
    type.processSlotRefs(
        static_cast<AObj<RObjOrFakePtr>*>(&sampleArray)->begin(),
        kSkipGcStripeIndex,
        type.userPointerCount(),
        [&nPtr](RObjOrFakePtr&) { ++nPtr; } // count even if sample holds a null
    );
    m_numPointersPerSlot = nPtr;
  }

  // Has every work item been grabbed by some thread? This is of course
  // racy but a "true" return value can be trusted.
  bool allWorkTaken() const {
    return m_index.load(std::memory_order_relaxed) >= m_count;
  }

  // Post worker tasks to the executor.
  void spawnWorkers() {
    summonAncestors();

    auto workers = getWorkers();

    for (auto n = std::min<size_t>(s_numThreads, m_count); n != 0; --n) {
      if (allWorkTaken()) {
        // Threads already grabbed all the work, don't spawn more threads.
        break;
      }

      workers->addTask(
          [tab = Tabulate::Ptr{this}]() { tab->runWorkerThread(); });
    }
  }

  // We might be in an arbitrarily nested parallelTabulate.
  // We don't want an ancestor thread running parallelTabulate to
  // run out of work to do and sit idle, indirectly waiting on us to
  // finish. Instead, we'd rather it jump in and help its grandchildren
  // (or whatever). The sooner its descendants finish their work, the
  // sooner the "outer" parallelTabulate finishes.
  //
  // To accomplish this, we post a task to it.
  //
  // If the parent doesn't see the task until all the work here is done,
  // no big deal, it will immediately realize that and drop it.
  void summonAncestors() {
    if (auto parent = Process::cur()->m_parent) {
      parent.schedule([tab = Tabulate::Ptr{this}]() {
        if (!tab->allWorkTaken()) {
          // Recursively summon its ancestors as well.
          tab->summonAncestors();
          tab->runWorkerThread();
        }
      });
    }
  }

  // Wait for all worker threads to finish.
  void waitForWorkers() {
    // NOTE: While waiting, we may help a descendant finish its own
    // parallelTabulate (see summonAncestors()).
    while (m_numActiveThreads.load(std::memory_order_acquire) != 0) {
      Process::cur()->runExactlyOneTaskSleepingIfNecessary();
    }
  }

  void checkForException() {
    if (auto thrower = std::move(m_thrower)) {
      // An exception was thrown.
      Process::cur()->joinChild(*thrower->m_process, m_resultNote);

      const auto exception = thrower->m_exception;
      const auto exitExceptionStatus = thrower->m_exitExceptionStatus;
      thrower.reset();

      // Rethrow the exception.
      if (exception) {
        throw SkipException(exception);
      } else {
        throw SkipExitException{exitExceptionStatus};
      }
    }
  }

  // Transfers w's results from its m_results array to the master result
  // array, in preparation for w being destroyed.
  void copyResults(TabulateWorker& w) {
    auto master = Process::cur();
    if (w.m_results.empty()) {
      master->sweepChild(*w.m_process);
      master->joinChild(*w.m_process, m_resultNote);
      return;
    }

    if (m_results == nullptr) {
      // Now that we have a one-element Array containing a result value, we
      // have the vtable we need to allocate the full result Array (they
      // have the same type).
      allocResults(*w.m_results.back().first);
    }

    // Copy each value from its one-element Array into its slot in the
    // final Array. Since they are both Arrays of the same type their
    // slots have the same layout and we can just use memcpy(), even for
    // complex types like value classes.
    for (auto result : w.m_results) {
      memcpy(
          mem::add(m_results, result.second * m_slotSize),
          result.first,
          m_slotSize);
    }

    if (!m_numPointersPerSlot) {
      // no pointers escaped from the worker's heap
      master->sweepChild(*w.m_process);
    }
    Process::cur()->joinChild(*w.m_process, m_resultNote);
  }

 public:
  // Next work index to hand out.
  std::atomic<int64_t> m_index;

  // This starts out as -1, meaning no threads have started. The first thread
  // to start sets it to 1, and each other thread increments it.
  // As threads exit, they set decrement it.
  std::atomic<size_t> m_numActiveThreads;

  std::atomic<size_t> m_refcount;

  // The total number of indices to pass to m_closure: [0, 1, 2, ..., count).
  const int64_t m_count;

  // The user code to invoke.
  RObj* const m_closure;

  AObjBase* m_results = nullptr;

  // Cached info about the GC layout for slots in m_results.
  size_t m_numPointersPerSlot = 0;
  size_t m_slotSize = 0;

  // Memoizer context for the tabulate call. All threads should dump
  // discovered dependencies into this.
  Context* const m_ctx;

  // Master Process where all results go.
  UnownedProcess m_masterProcess;
  const SkipObstackPos m_resultNote;

  // The exception thrower with the lowest results index, if any seen yet.
  std::unique_ptr<TabulateWorker> m_thrower;
};

void intrusive_ptr_add_ref(Tabulate* t) {
  t->m_refcount.fetch_add(1, std::memory_order_relaxed);
}

void intrusive_ptr_release(Tabulate* t) {
  if (t->m_refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete t;
  }
}

TabulateWorker::TabulateWorker(Tabulate* master)
    : m_master(master),
      m_process(Process::make(
          master->m_masterProcess,
          master->m_resultNote,
          master->m_ctx)) {}

// Processes all available array entries.
void TabulateWorker::run() {
  ProcessContextSwitcher guard{m_process};

  auto noteAfterLastGC = Obstack::cur().note();
  size_t numResultsAfterLastGC = 0;

  while (true) {
    const auto index = m_master->nextIndex();
    if (index >= m_master->m_count) {
      break;
    }

    // Invoke the user's code to compute one value. Catches all exceptions.
    if (auto val = computeOne(index)) {
      // Record the result.
      m_results.emplace_back(val, index);

      // Only bother processing our Obstack if we have a nontrivial amount
      // of memory on it. The intent is to avoid pathological behavior where
      // we GC way too often for simple, fast m_closure functions.
      //
      // The size estimate here is arbitrary, but should be small enough
      // that it fits in the cache.
      if (Obstack::cur().usage(noteAfterLastGC) >= 12000) {
        // GC all memory since the last GC. Note that all bytes surviving
        // earlier GCs are still live -- they will go to the master -- so
        // we won't be visiting them at all.
        const size_t numToGC = m_results.size() - numResultsAfterLastGC;
        std::vector<RObjOrFakePtr> ptrs(numToGC);

        for (size_t i = 0; i < numToGC; ++i) {
          ptrs[i].setPtr(m_results[numResultsAfterLastGC + i].first);
        }

        Obstack::cur().collect(noteAfterLastGC, ptrs.data(), ptrs.size());

        for (size_t i = 0; i < numToGC; ++i) {
          m_results[numResultsAfterLastGC + i].first = ptrs[i].unsafeAsPtr();
        }

        numResultsAfterLastGC = m_results.size();
        noteAfterLastGC = Obstack::cur().note();
      }

      // If anyone asked us to do any work, do it now and let other
      // threads worry about the work queue items.
      m_process->runReadyTasks();
    } else {
      // An exception was thrown, give up.
      break;
    }
  }
}

void TabulateWorker::finishWithException(
    int64_t index,
    RObj* skipException,
    int exitExceptionStatus) {
  // Convince other threads to not bother computing any answers
  // at indices larger than ours, since there's an exception anyway.
  m_master->m_index = m_master->m_count;

  // Record what exception we found.
  m_exceptionIndex = index;
  m_hasException = true;
  m_exception = skipException;
  m_exitExceptionStatus = exitExceptionStatus;

  // We don't need these any more.
  m_results.clear();
}

AObjBase* TabulateWorker::computeOne(int64_t index) {
  try {
    return m_master->callClosure(index);
  } catch (SkipException& exc) {
    finishWithException(index, exc.m_skipException, 0);
  } catch (SkipExitException& exc) {
    finishWithException(index, nullptr, exc.m_status);
  } catch (std::exception& exc) {
    // TODO: Convert this to a Skip exception? But note that in the
    // normal course of events an arbitrary C++ exception would not
    // be catchable by Skip code and would kill the process anyway.
    fatal("Caught non-skip exception: %s", exc.what());
  } catch (...) {
    fatal("Caught unknown exception");
  }

  // Indicate that we caught an exception.
  return nullptr;
}

RObj* parallelTabulate(int64_t count, RObj* closure) {
  // This algorithm only works if it gets at least one result, so it
  // can steal the vtable.
  if (count <= 0 || static_cast<arraysize_t>(count) != count) {
    SKIP_throwInvariantViolation(String{"Illegal tabulate count"});
  }

  Tabulate::Ptr t{new Tabulate(count, closure), false};
  return t->runMasterThread();
}
} // anonymous namespace

void skip::setNumThreads(size_t sz) {
  s_numThreads = sz;
}

size_t skip::getNumThreads() {
  return s_numThreads;
}

RObj* SKIP_parallelTabulate(SkipInt count, RObj* closure) {
  return parallelTabulate(count, closure);
}

SkipInt SKIP_numThreads() {
  return getNumThreads();
}
