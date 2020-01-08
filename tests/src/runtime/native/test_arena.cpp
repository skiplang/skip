/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Arena.h"
#include "skip/memory.h"

#include "testutil.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <random>
#include <vector>

#include <boost/thread/barrier.hpp>

using namespace skip;
using namespace skip::test;

TEST(ArenaTest, testSimple) {
  // Try some simple allocations

  auto p0 = Arena::alloc(16, Arena::Kind::iobj);
  Arena::free(p0);

  p0 = Arena::alloc(16, Arena::Kind::iobj);
  EXPECT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind(p0));
  // NOTE: It would be nice to test this but it's not necessarily true -
  // it's up to the Arena how to partition memory.
  // EXPECT_NE(Arena::Kind::iobj, Arena::getMemoryKind((char*)p0 - 1));
  EXPECT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind((char*)p0 + 8));
  EXPECT_EQ(Arena::Kind::unknown, Arena::rawMemoryKind(&p0));
  Arena::free(p0);

  p0 = Arena::calloc(16, Arena::Kind::iobj);
  EXPECT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind(p0));
  EXPECT_TRUE(((uint64_t*)p0)[0] == 0);
  EXPECT_TRUE(((uint64_t*)p0)[1] == 0);
  Arena::free(p0);

  auto p1 = Arena::alloc(16, Arena::Kind::iobj);
  auto p2 = Arena::alloc(16, Arena::Kind::iobj);
  Arena::free(p1);
  Arena::free(p2);

  // Try to allocate a lot
  std::vector<void*> ptrs(4096);
  for (auto& p : ptrs) {
    p = Arena::alloc(512, Arena::Kind::iobj);
    EXPECT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind(p));
  }
  std::random_device rd;
  std::mt19937 g(rd());

  // free in random order
  std::shuffle(ptrs.begin(), ptrs.end(), g);
  for (auto p : ptrs) {
    Arena::free(p);
  }
}

TEST(ArenaTest, testAligned) {
  std::vector<void*> ptrs;

  // Try some allocations in order from small to big so that it's more likely
  // to screw up accidental alignment.

  for (auto sz : std::vector<size_t>{1, 2, 4, 8, 16, 32, 64, 128, 256, 4096}) {
    void* p = Arena::allocAligned(sz, sz, Arena::Kind::iobj);
    EXPECT_EQ((reinterpret_cast<uintptr_t>(p)) & (sz - 1), 0U);
    ptrs.push_back(p);
  }

  for (auto p : ptrs) {
    Arena::free(p);
  }
}

TEST(ArenaTest, testKindMapper) {
  constexpr size_t CHUNK_SIZE = 1ULL << 21;
  auto& mapper = Arena::KindMapper::singleton();
  LameRandom random(42);

  std::array<Arena::Kind, sizeof(size_t) * 8 * 3> check;
  check.fill(Arena::Kind::unknown);
  void* base = mem::reserve(check.size() * CHUNK_SIZE, CHUNK_SIZE);

  for (size_t i = 0; i < 1000; ++i) {
    size_t idx = random.next(check.size());
    size_t n = random.next(sizeof(size_t) * 8 * 2) + 1;
    if (idx + n > check.size())
      n = check.size() - idx;
    Arena::Kind kind = (Arena::Kind)(random.next(3) + 1);

    for (size_t j = 0; j < n; ++j) {
      if (check[idx + j] != Arena::Kind::unknown) {
        mapper.erase(
            mem::add(base, (idx + j) * CHUNK_SIZE),
            mem::add(base, (idx + j + 1) * CHUNK_SIZE));
      }
      check[idx + j] = kind;
    }

    mapper.set(
        mem::add(base, idx * CHUNK_SIZE),
        mem::add(base, (idx + n) * CHUNK_SIZE),
        kind);

    for (size_t j = 0; j < check.size(); ++j) {
      EXPECT_EQ(check[j], mapper.get(mem::add(base, j * CHUNK_SIZE)));
    }
  }

  mapper.erase(base, mem::add(base, check.size() * CHUNK_SIZE));
  mem::free(base, check.size() * CHUNK_SIZE);
}

TEST(ArenaTest, testTorture) {
  const size_t MAX_ALLOC = 1ULL << 22; // 4MB - twice as big as a jemalloc chunk

  const auto duration = std::chrono::seconds(5);

  std::vector<void*> mem;

  LameRandom random(42);

  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <= duration) {
    int op = random.next(3);
    switch (op) {
      case 0: {
        // alloc
        size_t len = random.next64(MAX_ALLOC - 1) + 1;
        void* p = nullptr;
        try {
          p = Arena::alloc(len, Arena::Kind::iobj);
        } catch (std::bad_alloc&) {
          // this is expected occasionally when we exceed total memory
          continue;
        }
        memset(p, 0, len);
        assert(p != nullptr);
        ASSERT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind(p));
        ASSERT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind((char*)p + len / 2));
        ASSERT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind((char*)p + len - 1));
        mem.push_back(p);
      } break;

      case 1: {
        if (!mem.empty()) {
          // free
          size_t index = random.next64(mem.size());
          ASSERT_EQ(Arena::Kind::iobj, Arena::rawMemoryKind(mem[index]));
          Arena::free(mem[index]);
          mem.erase(mem.begin() + index);
        }
      } break;

      case 2: {
        // alloc local
        size_t len = random.next64(MAX_ALLOC - 1) + 1;
        void* p = malloc(len);
        ASSERT_EQ(Arena::Kind::unknown, Arena::rawMemoryKind(p));
        free(p);
      } break;
    }
  }

  // NOTE: Deleting the Arena deletes all the memory the arena knows about
}

TEST(ArenaTest, testThread) {
  // In several threads allocate memory and ensure that the same memory isn't
  // given to multiple threads.

  const size_t kAllocCount = 1000;
  const size_t kAllocSize = 4096;
  const size_t kIterations = 25;
  const size_t kNumThreads = 8;

  std::array<std::thread, kNumThreads> threads;
  boost::barrier barrier{kNumThreads};
  std::map<void*, int> allocations;
  std::mutex mutex;

  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i] = std::thread(
        [&](const int threadId) {
          for (size_t iteration = 0; iteration < kIterations; ++iteration) {
            barrier.wait();

            std::array<void*, kAllocCount> ptrs;
            for (size_t n = 0; n < kAllocCount; ++n) {
              ptrs[n] = Arena::alloc(kAllocSize, Arena::Kind::iobj);
            }

            barrier.wait();

            for (size_t n = 0; n < kAllocCount; ++n) {
              Arena::free(ptrs[n]);
            }

            {
              std::unique_lock<std::mutex> lock(mutex);
              for (size_t n = 0; n < kAllocCount; ++n) {
                auto it = allocations.insert({ptrs[n], threadId});
                EXPECT_EQ(it.first->second, threadId);
              }
            }

            barrier.wait();

            if (threadId == 0) {
              allocations.clear();
            }
          }
        },
        i);
  }

  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i].join();
  }
}
