/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

// #define ENABLE_DEBUG_TRACE 1

#if ENABLE_DEBUG_TRACE

#include <iostream>
#include <sstream>
#include <thread>

// This uses a temporary stringstream to reduce thread log interleaving.
#define DEBUG_TRACE(...)                                                      \
  do {                                                                        \
    std::ostringstream dtsstr_;                                               \
    dtsstr_ << "thread " << std::this_thread::get_id() << ": " << __VA_ARGS__ \
            << '\n';                                                          \
    std::cerr << dtsstr_.str() << std::flush;                                 \
  } while (false)

#else

#define DEBUG_TRACE(...) \
  do {                   \
  } while (false)

#endif

namespace skip {

class noncopyable {
  protected:
    noncopyable() = default;
    ~noncopyable() = default;
    noncopyable( const noncopyable& ) = delete;
    noncopyable& operator=( const noncopyable& ) = delete;
};

void printStackTrace();

// Parse numeric environment variables
uint64_t parseEnv(const char* name, uint64_t defaultVal);
double parseEnvD(const char* name, double defaultVal);

/**
 * Swirl around and mix together the bits in n. Useful for hashing.
 */
inline size_t mungeBits(size_t n) {
  // Multiply puts nicely mixed bits into the high bits, and byteswapping moves
  // them back down in a funny nonlinear way. We multiply by a value found via
  // random trial and error.
  //
  // See http://locklessinc.com/articles/fast_hash for more information.

  // This value was found to be a worst case of 8.20% error via
  // https://fburl.com/gi0mdexj
  // (when biased to only care about the low 22 bits)
  constexpr uint64_t multiplier64 = 0x0bd318c59d3984adULL;
  static_assert(sizeof(size_t) == 8, "unhandled case");
#ifdef __SIZEOF_INT128__
  // The high bits of a 64x64 -> 128 multiply will be well mixed.
  using uint128_t = unsigned __int128;
  const uint128_t m = (uint128_t)n * multiplier64;
  uint64_t mul = (uint64_t)m + ((uint64_t)(m >> 64));
#else
#error error
#endif

  // Swap endianness
  mul = ((mul & 0x00000000FFFFFFFFull) << 32) |
      ((mul & 0xFFFFFFFF00000000ull) >> 32);
  mul = ((mul & 0x0000FFFF0000FFFFull) << 16) |
      ((mul & 0xFFFF0000FFFF0000ull) >> 16);
  mul = ((mul & 0x00FF00FF00FF00FFull) << 8) |
      ((mul & 0xFF00FF00FF00FF00ull) >> 8);

  return mul;
}

/**
 * Combines two hash values into a new value derived from both.
 */
inline size_t hashCombine(size_t seed, size_t v) {
  return mungeBits(seed) + v;
}

namespace detail {

/// Internal implementation of rounding up either pointers or integers.
template <typename T, typename Enable = void>
struct RoundUp {};

/// Partial specialization for rounding up integers.
template <typename T>
struct RoundUp<
    T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type> {
  static constexpr T roundUp(T n, size_t align) {
    return static_cast<T>((n + (align - 1)) & -static_cast<T>(align));
  }
};

/// Partial specialization for rounding up pointers.
template <typename T>
struct RoundUp<T, typename std::enable_if<std::is_pointer<T>::value>::type> {
  static T roundUp(T n, size_t align) {
    return reinterpret_cast<T>(
        RoundUp<uintptr_t>::roundUp(reinterpret_cast<uintptr_t>(n), align));
  }
};

/// Internal implementation of rounding down either pointers or integers.
template <typename T, typename Enable = void>
struct RoundDown {};

/// Partial specialization for rounding down integers.
template <typename T>
struct RoundDown<
    T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type> {
  static constexpr T roundDown(T n, size_t align) {
    return static_cast<T>(n & -static_cast<T>(align));
  }
};

/// Partial specialization for rounding down pointers.
template <typename T>
struct RoundDown<T, typename std::enable_if<std::is_pointer<T>::value>::type> {
  static T roundDown(T n, size_t align) {
    return reinterpret_cast<T>(
        RoundDown<uintptr_t>::roundDown(reinterpret_cast<uintptr_t>(n), align));
  }
};
} // namespace detail

/**
 * Rounds up n (either a pointer or integer) to the next multiple af align,
 * which must be a power of two.
 */
template <typename T>
constexpr T roundUp(T n, size_t align) {
  return detail::RoundUp<T>::roundUp(n, align);
}

/**
 * Rounds down n (either a pointer or integer) to the prior multiple af align,
 * which must be a power of two.
 */
template <typename T>
constexpr T roundDown(T n, size_t align) {
  return detail::RoundDown<T>::roundDown(n, align);
}

struct pair_hash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2>& pair) const {
    return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
  }
};

/// Hash a block of memory.
size_t hashMemory(const void* p, size_t size, size_t seed = ~0);

// If msg2 is non-null then prints both messages separated by a colon.
[[noreturn]] void fatal(const char* msg, const char* msg2 = nullptr);
// Like fatal() but prints "<msg>: <strerror(errno)>"
[[noreturn]] void errnoFatal(const char* msg);

/// Like posix_memalign(), but dies if out of memory.
void* allocAligned(size_t size, size_t align) _MALLOC_ALIGN_ATTR(1, 2);

/// CRTP base class for types with higher-than-usual alignment.
template <typename Derived, ssize_t alignment = -1>
struct Aligned : private skip::noncopyable {
  void* operator new(size_t size) {
    return allocAligned(size, alignment == -1 ? alignof(Derived) : alignment);
  }

  void operator delete(void* p) {
    ::free(p);
  }
};

bool equalBytesExpectingYes(const void* a, const void* b, size_t size);

// Throws a runtime error with a message built from a formatted string.
// Note - calls va_end() on ap.
void throwRuntimeErrorV(const char* msg, va_list ap)
    __attribute__((__noreturn__));

void throwRuntimeError(const char* msg, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));

struct SpinLock {
  std::atomic<uint8_t> m_bits;
  void init();
  void lock();
  void unlock();
};

int findFirstSet(unsigned long n);
int findLastSet(unsigned long n);
std::string escape_json(const std::string& s);

template <class T>
inline constexpr bool isPowTwo(T const v) {
  static_assert(std::is_integral<T>::value, "non-integral type");
  static_assert(std::is_unsigned<T>::value, "signed type");
  static_assert(!std::is_same<T, bool>::value, "bool type");
  return (v != 0) && !(v & (v - 1));
}

template <class T>
inline T loadUnaligned(const void* p) {
  T value;
  memcpy(&value, p, sizeof(T));
  return value;
}

class StringPiece {
 public:
  StringPiece(char* begin, char* end) : m_begin(begin), m_end(end) {}

  char front() {
    return *m_begin;
  }

  char* begin() {
    return m_begin;
  }

  char* end() {
    return m_end;
  }

  void pop_front() {
    m_begin++;
  }

  bool empty() {
    return m_begin == m_end;
  }

  size_t size() {
    return m_end - m_begin;
  }

  char* m_begin;
  char* m_end;
};

} // namespace skip
