// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SPINNING_MUTEX_H_
#define PARTITION_ALLOC_SPINNING_MUTEX_H_

#include <algorithm>
#include <atomic>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/yield_processor.h"

#if PA_BUILDFLAG(IS_WIN)
#include "partition_alloc/partition_alloc_base/win/windows_types.h"
#endif

#if PA_BUILDFLAG(IS_POSIX)
#include <pthread.h>

#include <cerrno>
#endif

#if PA_BUILDFLAG(IS_APPLE)

#include <os/lock.h>

// os_unfair_lock is available starting with OS X 10.12, and Chromium targets
// 10.11 at the minimum, so the symbols are not always available *at runtime*.
// But we build with a 11.x SDK, so it's always in the headers.
//
// However, since the majority of clients have at least 10.12 (released late
// 2016), we declare the symbols here, marking them weak. They will be nullptr
// on 10.11, and defined on more recent versions.

// Silence the compiler warning, here and below.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

#define PA_WEAK __attribute__((weak))

extern "C" {

PA_WEAK void os_unfair_lock_lock(os_unfair_lock_t lock);
PA_WEAK bool os_unfair_lock_trylock(os_unfair_lock_t lock);
PA_WEAK void os_unfair_lock_unlock(os_unfair_lock_t lock);

}  // extern "C"

#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_APPLE)

#if PA_BUILDFLAG(IS_FUCHSIA)
#include <lib/sync/mutex.h>
#endif

namespace partition_alloc::internal {

// The behavior of this class depends on platform support:
// 1. When platform supports is available:
//
// Simple spinning lock. It will spin in user space a set number of times before
// going into the kernel to sleep.
//
// This is intended to give "the best of both worlds" between a SpinLock and
// base::Lock:
// - SpinLock: Inlined fast path, no external function calls, just
//   compare-and-swap. Short waits do not go into the kernel. Good behavior in
//   low contention cases.
// - base::Lock: Good behavior in case of contention.
//
// We don't rely on base::Lock which we could make spin (by calling Try() in a
// loop), as performance is below a custom spinlock as seen on high-level
// benchmarks. Instead this implements a simple non-recursive mutex on top of:
// - Linux   : futex()
// - Windows : SRWLock
// - MacOS   : os_unfair_lock
// - POSIX   : pthread_mutex_trylock()
//
// The main difference between this and a libc implementation is that it only
// supports the simplest path: private (to a process), non-recursive mutexes
// with no priority inheritance, no timed waits.
//
// As an interesting side-effect to be used in the allocator, this code does not
// make any allocations, locks are small with a constexpr constructor and no
// destructor.
//
// 2. Otherwise: This is a simple SpinLock, in the sense that it does not have
//    any awareness of other threads' behavior.
class PA_LOCKABLE PA_COMPONENT_EXPORT(PARTITION_ALLOC) SpinningMutex {
 public:
  inline constexpr SpinningMutex();
  PA_ALWAYS_INLINE void Acquire() PA_EXCLUSIVE_LOCK_FUNCTION();
  PA_ALWAYS_INLINE void Release() PA_UNLOCK_FUNCTION();
  PA_ALWAYS_INLINE bool Try() PA_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void AssertAcquired() const {}  // Not supported.
  void Reinit() PA_UNLOCK_FUNCTION();

 private:
  PA_NOINLINE void AcquireSpinThenBlock() PA_EXCLUSIVE_LOCK_FUNCTION();
  void LockSlow() PA_EXCLUSIVE_LOCK_FUNCTION();

  // See below, the latency of PA_YIELD_PROCESSOR can be as high as ~150
  // cycles. Meanwhile, sleeping costs a few us. Spinning 64 times at 3GHz would
  // cost 150 * 64 / 3e9 ~= 3.2us.
  //
  // This applies to Linux kernels, on x86_64. On ARM we might want to spin
  // more.
  static constexpr int kSpinCount = 64;

#if PA_CONFIG(HAS_LINUX_KERNEL)
  void FutexWait();
  void FutexWake();

  static constexpr int kUnlocked = 0;
  static constexpr int kLockedUncontended = 1;
  static constexpr int kLockedContended = 2;

  std::atomic<int32_t> state_{kUnlocked};
#elif PA_BUILDFLAG(IS_WIN)
  PA_CHROME_SRWLOCK lock_ = SRWLOCK_INIT;
#elif PA_BUILDFLAG(IS_APPLE)
  os_unfair_lock unfair_lock_ = OS_UNFAIR_LOCK_INIT;
#elif PA_BUILDFLAG(IS_POSIX)
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
#elif PA_BUILDFLAG(IS_FUCHSIA)
  sync_mutex lock_;
#else
  std::atomic<bool> lock_{false};

#if BUILDFLAG(IS_APPLE)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
  os_unfair_lock unfair_lock_ = OS_UNFAIR_LOCK_INIT;
#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_APPLE)

  // Spinlock-like, fallback.
  PA_ALWAYS_INLINE bool TrySpinLock();
  PA_ALWAYS_INLINE void ReleaseSpinLock();
  void LockSlowSpinLock();
#endif  // PA_CONFIG(HAS_FAST_MUTEX)
};

PA_ALWAYS_INLINE void SpinningMutex::Acquire() {
  // Not marked `[[likely]]`, as:
  // 1. We don't know how much contention the lock would experience
  // 2. This may lead to weird-looking code layout when inlined into a caller
  // with `[[(un)likely]]` attributes.
  if (Try()) {
    return;
  }

  return AcquireSpinThenBlock();
}

inline constexpr SpinningMutex::SpinningMutex() = default;

#if PA_CONFIG(HAS_LINUX_KERNEL)

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  // Using the weak variant of compare_exchange(), which may fail spuriously. On
  // some architectures such as ARM, CAS is typically performed as a LDREX/STREX
  // pair, where the store may fail. In the strong version, there is a loop
  // inserted by the compiler to retry in these cases.
  //
  // Since we are retrying in Lock() anyway, there is no point having two nested
  // loops.
  int expected = kUnlocked;
  return (state_.load(std::memory_order_relaxed) == expected) &&
         state_.compare_exchange_weak(expected, kLockedUncontended,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed);
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  if (state_.exchange(kUnlocked, std::memory_order_release) == kLockedContended)
      [[unlikely]] {
    // |kLockedContended|: there is a waiter to wake up.
    //
    // Here there is a window where the lock is unlocked, since we just set it
    // to |kUnlocked| above. Meaning that another thread can grab the lock
    // in-between now and |FutexWake()| waking up a waiter. Aside from
    // potentially fairness, this is not an issue, as the newly-awaken thread
    // will check that the lock is still free.
    //
    // There is a small pessimization here though: if we have a single waiter,
    // then when it wakes up, the lock will be set to |kLockedContended|, so
    // when this waiter releases the lock, it will needlessly call
    // |FutexWake()|, even though there are no waiters. This is supported by the
    // kernel, and is what bionic (Android's libc) also does.
    FutexWake();
  }
}

#elif PA_BUILDFLAG(IS_WIN)

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  return !!::TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&lock_));
}

#elif PA_BUILDFLAG(IS_APPLE)

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  return os_unfair_lock_trylock(&unfair_lock_);
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  return os_unfair_lock_unlock(&unfair_lock_);
}

#elif PA_BUILDFLAG(IS_POSIX)

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  int retval = pthread_mutex_trylock(&lock_);
  PA_DCHECK(retval == 0 || retval == EBUSY);
  return retval == 0;
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  int retval = pthread_mutex_unlock(&lock_);
  PA_DCHECK(retval == 0);
}

#elif PA_BUILDFLAG(IS_FUCHSIA)

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  return sync_mutex_trylock(&lock_) == ZX_OK;
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  sync_mutex_unlock(&lock_);
}

#else

PA_ALWAYS_INLINE bool SpinningMutex::TrySpinLock() {
  // Possibly faster than CAS. The theory is that if the cacheline is shared,
  // then it can stay shared, for the contended case.
  return !lock_.load(std::memory_order_relaxed) &&
         !lock_.exchange(true, std::memory_order_acquire);
}

PA_ALWAYS_INLINE void SpinningMutex::ReleaseSpinLock() {
  lock_.store(false, std::memory_order_release);
}

#if BUILDFLAG(IS_APPLE)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"

PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  // ARM64 macOS is macOS 11.x at least, guaranteed to have os_unfair_lock().
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_trylock(&unfair_lock_);
#else
  if (PA_LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_trylock(&unfair_lock_);

  return TrySpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_unlock(&unfair_lock_);
#else
  // Always testing trylock(), since the definitions are all or nothing.
  if (PA_LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_unlock(&unfair_lock_);

  return ReleaseSpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

PA_ALWAYS_INLINE void SpinningMutex::LockSlow() {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return os_unfair_lock_lock(&unfair_lock_);
#else
  if (PA_LIKELY(os_unfair_lock_trylock))
    return os_unfair_lock_lock(&unfair_lock_);

  return LockSlowSpinLock();
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
}

#pragma clang diagnostic pop

#else
PA_ALWAYS_INLINE bool SpinningMutex::Try() {
  return TrySpinLock();
}

PA_ALWAYS_INLINE void SpinningMutex::Release() {
  return ReleaseSpinLock();
}

PA_ALWAYS_INLINE void SpinningMutex::LockSlow() {
  return LockSlowSpinLock();
}

#endif  // BUILDFLAG(IS_APPLE)

#endif  // PA_CONFIG(HAS_FAST_MUTEX)

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_SPINNING_MUTEX_H_
