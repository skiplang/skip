/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <vector>

#include <gtest/gtest.h>

#include <thread>

#include "skip/Process.h"
#include "testutil.h"

using namespace skip;

class barrier {
public:
    explicit barrier(std::size_t iCount) : 
      mThreshold(iCount), 
      mCount(iCount), 
      mGeneration(0) {
    }

    void wait() {
        std::unique_lock<std::mutex> lLock{mMutex};
        auto lGen = mGeneration;
        if (!--mCount) {
            mGeneration++;
            mCount = mThreshold;
            mCond.notify_all();
        } else {
            mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; });
        }
    }

private:
    std::mutex mMutex;
    std::condition_variable mCond;
    std::size_t mThreshold;
    std::size_t mCount;
    std::size_t mGeneration;
};

// Make sure we run tasks in the reverse order they were scheduled.
TEST(Process, testSimpleOrdering) {
  auto process = Process::make();

  std::vector<int> answers;

  for (int i = 9; i >= 0; --i) {
    process->schedule([i, &answers]() { answers.push_back(i * i); });
  }
  EXPECT_EQ(answers.size(), 0);

  process->runReadyTasks();

  EXPECT_EQ(answers.size(), 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(answers[i], i * i);
  }
}

// Make sure we run tasks in the reverse order they were scheduled
// while the process's busy.
TEST(Process, testUnownedOrdering) {
  auto process = Process::make();
  auto unowned = UnownedProcess(process);
  std::vector<int> answers;

  unowned.schedule([&answers, &unowned]() {
    for (int i = 9; i >= 0; --i) {
      unowned.schedule([i, &answers]() { answers.push_back(i * i); });
    }
  });

  EXPECT_EQ(answers.size(), 0);

  // Try picking off some individual tasks.
  for (int j = 0; j < 3; ++j) {
    process->runExactlyOneTaskSleepingIfNecessary();
    EXPECT_EQ(answers.size(), j);
  }

  process->runReadyTasks();

  EXPECT_EQ(answers.size(), 10);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(answers[i], i * i);
  }
}

// clang-format off
ALLOW_PRIVATE_ACCESS{

static bool isSleeping(Process::Ptr& process) {
  return process->isSleeping();
}

};
// clang-format on

// Make sure we run tasks in the reverse order they were requested
// while the process's busy.
TEST(Process, testThreads) {
  auto process = Process::make();
  std::vector<int> answers;

  barrier barrier{2};

  auto thread = std::thread([&]() {
    barrier.wait();

    for (int i = 9; i >= 5; --i) {
      process->schedule([i, &answers]() { answers.push_back(i); });
    }
  });

  process->schedule([&]() {
    // Schedule our own tasks.
    for (int i = 14; i >= 10; --i) {
      process->schedule([i, &answers]() { answers.push_back(i); });
    }

    barrier.wait();

    // Other thread posts tasks here.

    thread.join();

    // Schedule some more.
    for (int i = 4; i >= 0; --i) {
      process->schedule([i, &answers]() { answers.push_back(i); });
    }
  });

  process->runReadyTasks();

  EXPECT_EQ(answers.size(), 15);
  for (int i = 0; i < 15; ++i) {
    EXPECT_EQ(answers[i], i);
  }
}

// Test synchronously waiting for a task to show up.
TEST(Process, testRunOne) {
  auto process = Process::make();

  int counter = 0;

  for (int repeat = 0; repeat < 3; ++repeat) {
    barrier barrier{2};

    auto thread = std::thread([&]() {
      barrier.wait();

      // Make sure the main thread has gone to sleep before we wake it.
      while (!PRIVATE::isSleeping(process)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }

      UnownedProcess(process).schedule([&counter]() { ++counter; });
    });

    process->schedule([&]() {
      // The other thread should still be waiting on its barrier.
      EXPECT_EQ(counter, repeat);

      // Awaken the other thread.
      barrier.wait();

      // Block until a task is available and run it.
      process->runExactlyOneTaskSleepingIfNecessary();

      EXPECT_EQ(counter, repeat + 1);
    });

    process->runReadyTasks();

    EXPECT_EQ(counter, repeat + 1);

    thread.join();
  }
}

TEST(Process, testRefcount) {
  auto process = Process::make();

  EXPECT_EQ(process->currentRefcount(), 1);

  {
    auto process2 = process;
    EXPECT_EQ(process->currentRefcount(), 2);
  }

  EXPECT_EQ(process->currentRefcount(), 1);
}
