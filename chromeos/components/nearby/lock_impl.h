// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LOCK_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_LOCK_IMPL_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "chromeos/components/nearby/lock_base.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::Lock implementation. Recursive lock.
class LockImpl : public LockBase {
 public:
  LockImpl();
  ~LockImpl() override;

 private:
  friend class LockImplTest;

  // location::nearby::Lock:
  void lock() override;
  void unlock() override;

  // chromeos::nearby::LockBase:
  bool IsHeldByCurrentThread() override;

  base::Lock real_lock_;
  base::Lock bookkeeping_lock_;
  base::PlatformThreadId owning_thread_id_ = base::kInvalidThreadId;
  size_t num_acquisitions_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LockImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_LOCK_IMPL_H_
