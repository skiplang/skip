/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/InternTable.h"

#include "skip/intern.h"
#include "skip/objects.h"
#include "skip/String.h"
#include "skip/Type.h"

#include <sys/mman.h>
#include <thread>

namespace skip {

InternTable& getInternTable() {
  static InternTable s_internTable;
  return s_internTable;
}

/// Tag bit in an InternPointer indicating its lock is held.
static constexpr uint32_t kHeld = 0x1;

/// All lock bits ORed together.
static constexpr uintptr_t kLockBitsMask = kHeld;

/**
 * See if two objects are equal, where both o1 and o2 are known to
 * be of this type. Equality is defined in the bitwise sense, except
 * for the special case of CycleHandle, where it does a deep recursive
 * comparison.
 *
 * NOTE: This method assumes it was called from the intern table, which
 * has confirmed the two objects have the same hash, so it is optimized
 * for the case where the objects are in fact equal.
 */
static bool equalIfSameVTable(const RObj& o1, IObj& o2) {
  auto& type = o1.type();

  switch (type.kind()) {
    case Type::Kind::string: {
      size_t size = static_cast<const LongString&>(o1).paddedSize();
      return equalBytesExpectingYes(&o1, &o2, size);
    }

    case Type::Kind::cycleHandle: {
      auto& handle1 = static_cast<CycleHandle&>(o1);
      auto& handle2 = static_cast<CycleHandle&>(o2);

      if (handle1.m_hash != handle2.m_hash) {
        return false;
      }

      return deepEqual(handle1.m_root, handle2.m_root);
    }

    case Type::Kind::array: {
      auto& a1 = static_cast<const AObjBase&>(o1);
      return (
          a1.arraySize() == o2.arraySize() &&
          equalBytesExpectingYes(&o1, &o2, o2.userByteSize()));
    }

    default:
      return equalBytesExpectingYes(&o1, &o2, o1.userByteSize());
  }
}

static bool objectsEqual(const RObj& o1, IObj& o2) {
  // They must have the same type (and for Strings, same layout and hash).
  return o1.vtable() == o2.vtable() && equalIfSameVTable(o1, o2);
}

/// A hash bucket in InternTable (i.e. an entry in its array).
// @lint-ignore HOWTOEVEN1
struct Bucket : private skip::noncopyable {
  friend InternTable;

  Bucket(const Bucket& other) : m_bits(other.m_bits) {}

  Bucket& operator=(const Bucket& other) = delete;

  void lock() {
    m_lock.lock();
  }

  void unlock() {
    m_lock.unlock();
  }

  /**
   * Find in this locked bucket an object equal to key with the given hash,
   * or return nullptr if none is found.
   */
  IObj* findAssumingLocked(const RObj& key, size_t hash) {
    // Accelerate comparisons by quickly checking to see if the extra hash bits
    // stashed in each InternPtr match the hash we are looking up. If they don't
    // match, the objects can't be equal. If they do match they are almost
    // certainly equal.
    const size_t mask = ((size_t)1 << InternPtr::kNumExtraHashBits) - 1;
    const size_t extraHashBits = (hash >> InternTable::kLog2MinBuckets) & mask;

    // Walk the linked list of buckets looking for a match.
    IObj* obj;
    for (auto b = m_ptr; (obj = b.ptr()) != nullptr; b = obj->internNext()) {
      // Only bother comparing against objects whose hashes match.
      if (b.extraHashBits() == extraHashBits && objectsEqual(key, *obj)) {
        return obj;
      }
    }

    return nullptr;
  }

  /**
   * Given that this bucket is locked and contains f, remove f from the
   * bucket and release the lock.
   */
  void eraseAndUnlock(IObj& f) {
    const Bucket head = *this;
    if (head.m_ptr.ptr() == &f) {
      // Splicing out the head of the list needs special atomicity.
      replaceHeadAndUnlock(head, f.internNext());
    } else {
      // Scan down the list looking for f so we can splice it out.
      InternPtr* prevp = &m_ptr;
      for (IObj* obj; (obj = prevp->ptr()) != &f; prevp = &obj->internNext()) {
        if (UNLIKELY(obj == nullptr)) {
          fatal("Attempted to erase missing object from intern table.");
        }
      }

      // Success! Splice f out from the list.
      *prevp = f.internNext();

      unlock();
    }

    // Clearing the next pointer is not strictly necessary.
    // TODO: Try a wacky garbage pointer here to catch bugs?
    f.internNext().assignPointerAndExtraHash(nullptr, 0);
  }

  /**
   * Inserts the given object known to have the specified hash into this
   * bucket then unlocks the bucket.
   *
   * The object must not already be stored in the InternTable.
   */
  void insertAndUnlock(IObj& obj, size_t hash) {
    // Chain obj to point to the old list head, prepending it.
    const Bucket head = *this;
    obj.internNext().assign(head.m_bits & ~kLockBitsMask);

    const size_t extraHashBits = hash >> InternTable::kLog2MinBuckets;

    // Create the new list head by combining obj and some hash bits.
    InternPtr newHead;
    newHead.assignPointerAndExtraHash(&obj, extraHashBits);

    // Drop in the new list head.
    replaceHeadAndUnlock(head, newHead);
  }

  std::vector<IObjPtr> getBucketContentsAndUnlock() {
    std::vector<IObjPtr> contents;
    IObj* obj;
    for (auto b = m_ptr; (obj = b.ptr()) != nullptr; b = obj->internNext()) {
      contents.push_back(obj);
    }
    unlock();
    return contents;
  }

  /**
   * Replace the head of a locked bucket list, known to equal oldv,
   * with newv.
   *
   */
  void replaceHeadAndUnlock(Bucket oldv, InternPtr newv) {
    const uintptr_t newBits = newv.bits() | kHeld;
    m_atomic.hi = newBits >> 32;
    const auto newlo = (uint32_t)newBits;
    m_atomic.lo = newlo;

    unlock();
  }

  union {
    uintptr_t m_bits;

    InternPtr m_ptr;

    SpinLock m_lock;

    struct {
      // Technically this is not C++11-compliant, since atomic<uint32_t> need
      // not have the same layout as an uintptr_t. But we don't care.
      std::atomic<uint32_t> lo;

      uint32_t hi;
    } m_atomic;
  };
};

/**
 * Returns the canonical empty list. It must have a nullptr pointer
 * but a nonzero tag, which distinguishes it from the all-zeros "needs
 * rehash" bucket value sentinel.
 */
static InternPtr emptyBucketList() {
  InternPtr p;
  p.assignPointerAndExtraHash(nullptr, 1);
  return p;
}

// See algorithm notes in InternTable.h.
InternTable::InternTable(int log2MaxBuckets)
    : m_mask(((size_t)1 << kLog2MinBuckets) - 1),
      m_maxBuckets((size_t)1 << log2MaxBuckets),
      m_size(0) {
  assert(log2MaxBuckets >= kLog2MinBuckets);
  assert(log2MaxBuckets <= kLog2MaxBuckets);

  // TODO: rework this to do page allocation like Arena
  void* buckets = mmap(
      nullptr,
      m_maxBuckets * sizeof(m_buckets[0]),
      PROT_READ | PROT_WRITE,
      MAP_ANON | MAP_PRIVATE | MAP_NORESERVE,
      -1,
      0);

  if (buckets == MAP_FAILED) {
    // TODO: Just die here? Don't want lots of code thinking about OOM.
    throw std::bad_alloc();
  }

  m_buckets = static_cast<Bucket*>(buckets);
  const size_t initialSize = m_mask + 1;

#ifdef MADV_DONTDUMP
  // Advise that we don't need to include the unused memory in our core dumps
  madvise(
      m_buckets + initialSize,
      (m_maxBuckets - initialSize) * sizeof(m_buckets[0]),
      MADV_DONTDUMP);
#endif

  // Set the initially visible buckets to empty, rather than the default
  // "need lazy rehash" sentinel bit pattern of all-zeros.
  for (size_t i = 0; i < initialSize; ++i) {
    m_buckets[i].m_ptr = emptyBucketList();
  }
}

InternTable::~InternTable() {
  int rc ATTR_UNUSED = munmap(m_buckets, m_maxBuckets * sizeof(m_buckets[0]));
  assert(rc == 0);
}

Bucket& InternTable::lockBucketIndex(size_t slot) {
  Bucket& b = m_buckets[slot];
  b.lock();

  if (UNLIKELY(b.m_ptr.isLazyRehashSentinel())) {
    rehash(slot);
  }

  return b;
}

void InternTable::eraseAndUnlock(Bucket& bucket, IObj& obj) {
  --m_size;
  bucket.eraseAndUnlock(obj);
}

void InternTable::reserve(size_t newSize) {
  size_t oldMask = m_mask.load(std::memory_order_relaxed);

  // Grow until we hit a load factor of 2/3. Note that this math can't
  // overflow, or we would already have exhausted our address space given
  // the byte size of each Bucket.
  if (UNLIKELY(newSize * 3 >= oldMask * 2)) {
    newSize = newSize * 3 / 2;
    size_t newMask = oldMask;
    do {
      newMask = (newMask << 1) + 1;
    } while (newMask <= newSize);
    if (newMask < m_maxBuckets) {
      // Growing will not exceed our maximum number of buckets. Try to
      // grow the mask, but if someone else beat us to it then just ignore it.
      if (m_mask.compare_exchange_strong(oldMask, newMask)) {
#ifdef MADV_DODUMP
        // Let the system know it should dump this new area out if we crash
        const size_t oldSize = oldMask + 1;
        madvise(
            m_buckets + oldSize, oldSize * sizeof(m_buckets[0]), MADV_DODUMP);
#endif
      }
    }
  }
}

void InternTable::insertAndUnlock(Bucket& bucket, IObj& obj, size_t hash) {
  bucket.insertAndUnlock(obj, hash);
  reserve(++m_size);
}

std::vector<IObjPtr> InternTable::internalGetBucketContentsAndUnlock(
    Bucket& bucket) {
  return bucket.getBucketContentsAndUnlock();
}

IObj* InternTable::findAssumingLocked(
    Bucket& bucket,
    const RObj& key,
    size_t hash) {
  return bucket.findAssumingLocked(key, hash);
}

void InternTable::unlockBucket(Bucket& bucket) {
  bucket.unlock();
}

/**
 * Set the locked bucket, known to be empty, to the new list,
 * leaving it locked.
 */
static void initializeBucket(Bucket& bucket, InternPtr value) {
  // Set high 32 bits non-atomically.
  const uintptr_t bits = value.bits();
  bucket.m_atomic.hi = (uint32_t)(bits >> 32);

  // We know the old bit pattern was zero for everything except
  // the lock bits, so we can set the other 30 bits atomically by simply
  // adding in the bit pattern we want.
  const uint32_t old ATTR_UNUSED = bucket.m_atomic.lo.fetch_add(
      (uint32_t)(bits & ~kLockBitsMask), std::memory_order_release);
}

void InternTable::rehash(size_t slot) {
  // This slot needs to be lazily rehashed into.

  // The rehashing parent is 'slot' with its highest bit turned off.
  // There must be some bit set in slot, or it wouldn't need rehashing.
  const auto highestBitIndex = skip::findLastSet(slot) - 1;
  const size_t parentSlot = slot - ((size_t)1 << highestBitIndex);

  // Recursively lock the parent.
  Bucket& parent = lockBucketIndex(parentSlot);

  //
  // Walk the parent's list, partitioning into those entries that
  // should stay in 'parentSlot' and those that should move to 'slot'.
  //
  // We partition by looking at the hash bits to see which objects in
  // parentSlot actually belong in slot.
  //
  // To make this decision we only consider the hash bits in partitionMask.

  // Any object with these extra hash bits should be moved.
  const size_t moveExtraHash = slot >> kLog2MinBuckets;

  // These are the relevant bits of extraHashBits() for determining
  // whether each object belongs in 'slot' or 'parentSlot'.
  const size_t partitionMask =
      ~(size_t)0 >> (63 - (highestBitIndex - kLog2MinBuckets));

  // These lists hold the new lists for { parentSlot, slot }.
  std::array<InternPtr, 2> newLists;
  std::array<InternPtr*, 2> tail = {{&newLists[0], &newLists[1]}};

  const Bucket oldParentHead = parent;
  for (InternPtr p = oldParentHead.m_ptr, next; p.ptr() != nullptr; p = next) {
    const bool move = ((p.extraHashBits() & partitionMask) == moveExtraHash);

    // Append to list 0 if the value stays in parentSlot, 1 if it moves to slot.
    //
    // We append rather than prepend since it's nice to preserve the property
    // that the more-recently added objects are near the head of each list.
    InternPtr& nextp = p.ptr()->internNext();
    next = nextp;
    *tail[move] = p;
    tail[move] = &nextp;
  }

  // Terminate both lists.
  *tail[0] = *tail[1] = emptyBucketList();

  // Update the new lists.
  initializeBucket(m_buckets[slot], newLists[1]);
  parent.replaceHeadAndUnlock(oldParentHead, newLists[0]);
}

Bucket& InternTable::lockHash(size_t hash) {
  for (size_t mask = m_mask.load(std::memory_order_relaxed);;) {
    Bucket& b = lockBucketIndex(hash & mask);

    const size_t newMask = m_mask.load(std::memory_order_relaxed);
    if (LIKELY(mask == newMask)) {
      return b;
    }

    // The hash table resized while we were locking, start over.
    b.unlock();
    mask = newMask;
  }
}

Bucket& InternTable::lockObject(IObj& obj) {
  // TODO: We may be able to accelerate this for the case of objects
  // already in the table.
  //
  // The idea is to say that the linked list of buckets is terminated
  // with a tagged "pointer" that really contains its hash. The low two
  // tag bits of the InternPtr are unused in internNext(), since they
  // are only for holding a lock in the Bucket. So in the case where an
  // object is "last" in its bucket, we could simply get the hash immediately
  // from the object.
  //
  // Unfortunately for other objects it's not safe to follow their
  // internNext() chain to the end to find the bucket, since that chain
  // might be getting rewritten by another thread. That could still be OK,
  // if we follow the pointer chain conservatively, making sure they remain
  // in the arena holding IObjs, and limiting the number of steps we take.
  return lockHash(obj.hash());
}

size_t InternTable::verifyInvariants() const {
  size_t longestCollisionList = 0;
  constexpr size_t minBuckets = (size_t)1 << kLog2MinBuckets;

  for (size_t slot = m_mask.load() + 1; slot-- > 0;) {
    Bucket& b = m_buckets[slot];

    if (b.m_ptr.isLazyRehashSentinel()) {
      assert(slot >= minBuckets);
      continue;
    }

    // Not a rehash sentinel. Make sure that its parent is not a rehash
    // sentinel, since that must be impossible.
    auto highestBitIndex = skip::findLastSet(slot) - 1;
    if (slot >= minBuckets) {
      size_t parentSlot ATTR_UNUSED = slot - ((size_t)1 << highestBitIndex);
      assert(!m_buckets[parentSlot].m_ptr.isLazyRehashSentinel());
    }

    if (b.m_ptr.ptr() == nullptr) {
      // Locking is extremely expensive, so if it looks like the list
      // is empty, don't bother locking and checking it.
      continue;
    }

    b.lock();

    // Figure out which bits of the hash we definitely know given that
    // objects are stored in this bucket.
    auto numKnownHashBits = std::max<int>(highestBitIndex + 1, kLog2MinBuckets);
    const size_t knownHashBitsMask ATTR_UNUSED =
        ((size_t)1 << numKnownHashBits) - 1;

    size_t listLength = 0;

    for (InternPtr p = b.m_ptr; p.ptr(); p = p.ptr()->internNext()) {
      ++listLength;

      IObj* const iobj = p.ptr();

      iobj->verifyInvariants();

      // Everything in the table must have a reasonable refcount.
      // Note that cycle members are not in here, although CycleHandles may be.
      const Refcount rc ATTR_UNUSED = iobj->currentRefcount();
      assert(rc > 0);
      assert(rc <= kMaxRefcount);

      // Everything in this bucket must have a hash that puts it here.
      // Technically we should look for higher-numbered non-sentinel
      // child buckets that these should have been rehashed into already.
      const size_t hash ATTR_UNUSED = iobj->hash();
      assert(((hash ^ slot) & knownHashBitsMask) == 0);

      // References can only point to other interned objects.
      iobj->eachValidRef(
          [&](IObj* ref ATTR_UNUSED) { assert(ref->isInterned()); });
    }

    longestCollisionList = std::max(longestCollisionList, listLength);

    b.unlock();
  }

  return longestCollisionList;
}

void InternTable::TEST_hardReset() {
  m_mask = ((size_t)1 << kLog2MinBuckets) - 1;
  m_size = 0;
}
} // namespace skip
