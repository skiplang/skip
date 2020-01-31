/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "SmallTaggedPtr.h"

#include <atomic>

#include <boost/functional/hash.hpp>

namespace skip {

/**
 * An IObj* plus some tag bits used by the InternTable.
 *
 * The low two bits are a skip::SpinLock, used to lock a Bucket.
 * They are only used in the head of the list, i.e. the pointer stored
 * directly in the buckets_ array.
 *
 * The remaining tag bits (17 on x86_64) store additional bits of the hash
 * of the pointed-to object, to accelerate lookups and rehashing.

 * Specifically, because an InternTable has at least
 * (1 << InternTable::kLog2MinBuckets) buckets, at least the low kLog2MinBuckets
 * of the hash are always implied by the bucket containing the object, and
 * all objects in that bucket must share those hash bits.
 *
 * Recording the extra hash bits "just above" the implied ones gives us
 * some useful information to accelerate rehashing, and lets us quickly weed
 * out objects on lookup that can't possibly be equal because their "extra"
 * hash bits do not match those of the full size_t hash being looked up.
 */
struct InternPtr final : SmallTaggedPtr<
                             IObj,
                             -1, // "max out" numTagBits
                             false, // loadBefore
                             false, // loadAfter
                             false, // pack
                             2 // numAlignBits: interned pointers are only
                               // guaranteed aligned mod 4.
                             > {
  /// The low two bits of each bucket are used as a skip::SpinLock.
  enum { kNumLockBits = 2 };

  enum { kNumExtraHashBits = kNumTagBits - kNumLockBits };

  size_t extraHashBits() const {
    // Tag bits beyond the lock bits are used to store extra bits of the hash
    // for the pointed-to object.
    return tag() >> kNumLockBits;
  }

  void assignPointerAndExtraHash(IObj* ptr, size_t extraHash) {
    assign(ptr, (extraHash << kNumLockBits) & kTagMask);
  }

  bool isLazyRehashSentinel() {
    // All bits except the two lock bits must be zero.
    return bits() < (1u << kNumLockBits);
  }
};

// TODO: Move this class to InternTable.cpp and just leave a few functions here?

/**
 * This is a hash table used to intern Skip objects (IObj*).
 *
 * It uses an intrusive linked list of objects per bucket with per-bucket
 * locking via a skip::SpinLock.
 *
 * Each pointer in the chain uses leftover "tag bits" to hold additional
 * bits of the hash of the pointed-to object. There is no need to even
 * compare against the object unless those tag bits match the corresponding
 * bits of the hash being looked up.
 *
 * The table uses an interesting lazy rehashing trick. Memory for all the
 * buckets it might ever use is mmap()ed up front, but most of the pages
 * are not "touched". Although this consumes a great deal of address space,
 * that is cheap on a 64-bit platform, and it does not actually consume any
 * physical memory (just a single struct vm_area_struct in the Linux kernel
 * for the entire range).
 *
 * A NULL value in a bucket (the default from the zero page shared by all
 * the untouched pages) means "this bucket needs to be rehashed into from its
 * parent lower-numbered bucket". An actual empty list uses a different
 * sentinel value. When a NULL bucket is locked, we also lock its parent
 * (found by turning off the highest set bit in the bucket index), then
 * partition the bucket entries between the two buckets. This process may
 * recurse if the parent is also NULL.
 *
 * When the table gets too full we simply double the number of active buckets
 * (i.e. the bit-AND mask applied to the hash to find the bucket), and do
 * nothing else. Over time the newly-accessible buckets will get lazily
 * populated with the correct contents when they are first accessed.
 */
struct InternTable final : private skip::noncopyable {
  explicit InternTable(int log2MaxBuckets = kLog2MaxBuckets);

  ~InternTable();

  void reserve(size_t newSize);

  /**
   * Locks the bucket with the given hash, lazily rehashing as needed.
   *
   * @return The newly-locked Bucket.
   */
  Bucket& lockHash(size_t hash);

  /**
   * Locks the bucket containing the given object, lazily rehashing as needed.
   *
   * @return The newly-locked Bucket.
   */
  Bucket& lockObject(IObj& obj);

  void unlockBucket(Bucket& bucket);

  void eraseAndUnlock(Bucket& bucket, IObj& obj);

  void insertAndUnlock(Bucket& bucket, IObj& obj, size_t hash);

  std::vector<IObjPtr> internalGetBucketContentsAndUnlock(Bucket& bucket);

  IObj* findAssumingLocked(Bucket& bucket, const RObj& key, size_t hash);

  size_t size() const {
    return m_size.load(std::memory_order_relaxed);
  }

  /**
   * Verify all of the table's internal invariants.
   *
   * @returns The length of the longest collision chain found.
   */
  size_t verifyInvariants() const;

  /// We always have at least 2**kLog2MinBuckets buckets.
#if ENABLE_VALGRIND
  // Valgrind can't handle more than 32GB of RAM - so lower our allocation size.
  enum {kLog2MinBuckets = 10};
#else
  // TODO: Workaround for T26658372: this could be 19 but it makes
  // core dumps enormous.
  enum { kLog2MinBuckets = 12 };
#endif

  size_t numberOfBuckets() const {
    return m_mask + 1;
  }

 private:
  friend Bucket;
  friend struct TestPrivateAccess;

  /// We can only store this many buckets. More than this and rehashing couldn't
  /// simply get additional hash bits from InternPtr::extraHashBits().
  enum { kLog2MaxBuckets = kLog2MinBuckets + InternPtr::kNumExtraHashBits };

  /**
   * Lock the given bucket, lazily rehashing as needed. Slots must only
   * be locked in order from higher to lower to avoid deadlock.
   *
   * @returns the newly-locked bucket.
   */
  Bucket& lockBucketIndex(size_t slot);

  /**
   * Rehashes into this slot from the lower-number slot that may have
   * entries that belong here. This is a key part of InternTable's lazy
   * rehashing scheme.
   */
  void rehash(size_t slot);

  Bucket* m_buckets;
  std::atomic<size_t> m_mask;
  const size_t m_maxBuckets;

  // Put size_ its own cache line since it will change frequently.
  char m_pad1[kCacheLineSize - sizeof(size_t)] FIELD_UNUSED;

  std::atomic<size_t> m_size;

  char m_pad2[kCacheLineSize - sizeof(std::atomic<size_t>)] FIELD_UNUSED;

  void TEST_hardReset();
};

/// Global singleton used to intern all objects in this process.
extern InternTable& getInternTable();
} // namespace skip
