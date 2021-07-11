/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/util.h"

#include "skip/Exception.h"
#include "skip/external.h"
#include "skip/VTable.h"

#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/uio.h>

#define UNW_LOCAL_ONLY 1
#include <libunwind.h>
#include <cxxabi.h>
#include <iomanip>
#include <thread>

#include <xmmintrin.h>

namespace skip {

void printStackTrace() {
  unw_cursor_t cursor;
  unw_context_t context;

  // Initialize cursor to current frame for local unwinding.
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  // Unwind frames one by one, going up the frame stack.
  while (unw_step(&cursor) > 0) {
    unw_word_t offset, pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0) {
      break;
    }
    fprintf(stderr, "  0x%.16lu:", pc);

    char sym[256];
    if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
      fprintf(stderr, " %s + 0x%lu\n", sym, offset);
    } else {
      fprintf(
          stderr, " -- error: unable to obtain symbol name for this frame\n");
    }
  }
}

uint64_t parseEnv(const char* name, uint64_t defaultVal) {
  auto s = std::getenv(name);
  return s ? strtoull(s, nullptr, 0) : defaultVal;
}

double parseEnvD(const char* name, double defaultVal) {
  auto s = std::getenv(name);
  return s ? strtod(s, nullptr) : defaultVal;
}

// NOTE: Hashing 0 bytes does not just return the seed. Is that OK?
size_t hashMemory(const void* p, size_t size, size_t seed) {
  auto m = static_cast<const uint8_t*>(p);
  size_t h = seed;

  size_t tail;

  if (UNLIKELY(size < sizeof(size_t))) {
    // Only a tiny number of bytes.
    tail = 0;

    if (sizeof(size_t) > 4 && (size & 4)) {
      tail = loadUnaligned<uint32_t>(m);
      m += 4;
    }
    if (size & 2) {
      tail = (tail << 16) | loadUnaligned<uint16_t>(m);
      m += 2;
    }
    if (size & 1) {
      tail = (tail << 8) | *m;
    }
  } else {
    // Hash a full word at a time.
    for (size_t i = 0; i < size - sizeof(size_t); i += sizeof(size_t)) {
      h = hashCombine(h, loadUnaligned<size_t>(m + i));
    }

    // Handle the last possibly full or possibly partial word by hashing
    // the last word, which perhaps overlaps some already hashed bytes,
    // but that is OK.
    tail = loadUnaligned<size_t>(m + size - sizeof(size_t));
  }

  return mungeBits(hashCombine(h, tail));
}

void fatal(const char* msg, const char* err) {
  std::array<struct iovec, 4> vec{{
      {const_cast<char*>(msg), strlen(msg)},
  }};
  size_t count = 1;
  if (err) {
    vec[count++] = {const_cast<char*>(": "), 2};
    vec[count++] = {const_cast<char*>(err), strlen(err)};
  }
  vec[count++] = {const_cast<char*>("\n"), 1};
  // This dumb construct is because GCC requires us to not ignore the return of
  // writev() but there's really nothing to do since we're aborting anyway.
  if (::writev(STDERR_FILENO, vec.data(), count) == -1) {
  }
  abort();
}

void errnoFatal(const char* msg) {
  const char* err = strerror(errno);
  fatal(msg, err);
}

/// Like posix_memalign(), but dies if out of memory.
void* allocAligned(size_t size, size_t align) {
  // The man page for ::posix_memalign() says that align must be a multiple of
  // sizeof(void*).
  if (align < sizeof(void*))
    align = sizeof(void*);
  assert(align % sizeof(void*) == 0);

  void* mem;
  int status = ::posix_memalign(&mem, align, size);
  if (UNLIKELY(status != 0)) {
    throw std::bad_alloc();
  }
  return mem;
}

/**
 * Returns true iff p1 and p2 point to identical blocks of memory,
 * optimized to assume that they are very likely to be equal.
 */
bool equalBytesExpectingYes(const void* a, const void* b, size_t size) {
  // TODO: We could theoretically do better than memcmp here, since we believe
  // they are equal, e.g. XOR words together a word at a time, OR those
  // values, and only check for zero at the end (perhaps using SSE etc.)
  // And maybe prefetch bytes.
  return LIKELY(memcmp(a, b, size) == 0);
}

void throwRuntimeErrorV(const char* const msg, va_list originalAp) {
  std::vector<char> buf(strlen(msg) * 2);
  bool retry = false;

  while (true) {
    va_list ap;
    va_copy(ap, originalAp);
    int size = vsnprintf(buf.data(), buf.size(), msg, ap);
    va_end(ap);
    if (size < 0) {
      va_end(originalAp);
      // vsnprintf() failed - just throw the message without the formatting.
      SKIP_throwRuntimeError(String(msg));
    }

    if ((size < buf.size()) || retry) {
      va_end(originalAp);
      buf.back() = '\0';
      SKIP_throwRuntimeError(String(buf.begin(), buf.begin() + size));
    }

    // 64k - Arbitrary limit on message length.
    buf.resize(std::min(size, 65535) + 1);
    retry = true;
  }
}

void throwRuntimeError(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  throwRuntimeErrorV(msg, ap);
}

void SpinLock::init() {
  const uint8_t newBits = m_bits & ~1;
  m_bits = newBits;
}

void SpinLock::lock() {
  auto spin_count = 0;

try_again:
  uint8_t bits = m_bits.load();
  uint8_t oldBits = bits & ~1;
  const uint8_t newBits = oldBits | 1;
  spin_count++;

  if (bits & 1) {
    if (spin_count % 16 == 0) {
      _mm_pause();
    } else {
      std::this_thread::yield();
    }
    goto try_again;
  }

  if (UNLIKELY(!m_bits.compare_exchange_weak(
          oldBits,
          newBits,
          std::memory_order_acquire,
          std::memory_order_relaxed))) {
    std::this_thread::yield();
    goto try_again;
  }
}

void SpinLock::unlock() {
  uint8_t oldBits = m_bits.load();
  const uint8_t newBits = oldBits & ~1;

  if ((oldBits & 1) == 0) {
    fprintf(stderr, "Internal error: spinlock double unlock\n");
    exit(70);
  }

  if (UNLIKELY(!m_bits.compare_exchange_strong(
          oldBits,
          newBits,
          std::memory_order_release,
          std::memory_order_relaxed))) {
    fprintf(
        stderr,
        "Internal error: spinlock in an impossible state %d\n",
        ((unsigned int)m_bits.load() & 2) != 0);
    exit(70);
  }
}

int findLastSet(unsigned long n) {
  unsigned long bitSize = sizeof(unsigned long) * 8LU;
  unsigned long bit = bitSize - 1LU;
  unsigned long r = 1LU << bit;

  while ((n & r) == 0LU) {
    if (bit == 0)
      return 0;
    bit--;
    r = 1ul << bit;
  }
  return bit + 1;
}

int findFirstSet(unsigned long n) {
  unsigned long bitSize = sizeof(unsigned long) * 8LU;
  unsigned long bit = 0LU;
  unsigned long r = 1LU << bit;

  while ((n & r) == 0LU) {
    if (bit >= bitSize)
      return 0;
    bit++;
    r = 1LU << bit;
  }
  return bit + 1;
}

std::string escape_json(const std::string& s) {
  std::ostringstream o;
  for (auto c = s.cbegin(); c != s.cend(); c++) {
    if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
      o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
    } else {
      o << *c;
    }
  }
  return o.str();
}

} // namespace skip
