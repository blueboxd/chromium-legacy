// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/lock_impl.h"

namespace chromeos {

namespace nearby {

LockImpl::LockImpl() = default;

LockImpl::~LockImpl() {
#if DCHECK_IS_ON()
  base::AutoLock al(bookkeeping_lock_);
  DCHECK_EQ(0u, num_acquisitions_);
  DCHECK_EQ(base::kInvalidThreadId, owning_thread_id_);
#endif
}

// NO_THREAD_SAFETY_ANALYSIS: Whether this locks or not depends on rumtime
// properties.
void LockImpl::lock() NO_THREAD_SAFETY_ANALYSIS {
  {
    base::AutoLock al(bookkeeping_lock_);
    if (num_acquisitions_ > 0u &&
        owning_thread_id_ == base::PlatformThread::CurrentId()) {
      real_lock_.AssertAcquired();
      ++num_acquisitions_;
      return;
    }
  }

  // At this point, either no thread currently holds |real_lock_|, in which case
  // the current thread should be able to immediately acquire it, or a different
  // thread holds it, in which case Acquire() will block. It's necessary that
  // Acquire() happens outside the critical sections of |bookkeeping_lock_|,
  // otherwise any future calls to unlock() will block on acquiring
  // |bookkeeping_lock_|, which would prevent Release() from ever running on
  // |real_lock_|, resulting in deadlock.
  real_lock_.Acquire();

  {
    base::AutoLock al(bookkeeping_lock_);
    DCHECK_EQ(0u, num_acquisitions_);
    owning_thread_id_ = base::PlatformThread::CurrentId();
    num_acquisitions_ = 1;
  }
}

// NO_THREAD_SAFETY_ANALYSIS: Whether this unlocks or not depends on rumtime
// properties.
void LockImpl::unlock() NO_THREAD_SAFETY_ANALYSIS {
  base::AutoLock al(bookkeeping_lock_);
  CHECK_GT(num_acquisitions_, 0u);
  DCHECK_EQ(base::PlatformThread::CurrentId(), owning_thread_id_);
  real_lock_.AssertAcquired();

  --num_acquisitions_;
  if (num_acquisitions_ == 0u) {
    owning_thread_id_ = base::kInvalidThreadId;
    real_lock_.Release();
  }
}

bool LockImpl::IsHeldByCurrentThread() {
  base::AutoLock al(bookkeeping_lock_);
  return owning_thread_id_ == base::PlatformThread::CurrentId();
}

}  // namespace nearby

}  // namespace chromeos
