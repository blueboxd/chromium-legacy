// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs_impl.h"

#include "base/time/time.h"

namespace updater {

class ScopedPrefsLockImpl {
 public:
  ScopedPrefsLockImpl() = default;
  ScopedPrefsLockImpl(const ScopedPrefsLockImpl&) = delete;
  ScopedPrefsLockImpl& operator=(const ScopedPrefsLockImpl&) = delete;
  ~ScopedPrefsLockImpl() = default;
};

ScopedPrefsLock::ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl)
    : impl_(std::move(impl)) {}

ScopedPrefsLock::~ScopedPrefsLock() = default;

std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    base::TimeDelta timeout) {
  // TODO(crbug.com/1092936): implement the actual mutex.
  return std::make_unique<ScopedPrefsLock>(
      std::make_unique<ScopedPrefsLockImpl>());
}

}  // namespace updater
