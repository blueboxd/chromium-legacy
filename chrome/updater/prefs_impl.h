// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PREFS_IMPL_H_
#define CHROME_UPDATER_PREFS_IMPL_H_

#include <memory>

namespace base {
class TimeDelta;
}  // namespace base

namespace updater {

class ScopedPrefsLockImpl;

// ScopedPrefsLock represents a held lock. Destroying the ScopedPrefsLock
// releases the lock. Implementors cannot depend on a ScopedPrefsLock being
// reentrant. The definition of ScopedPrefsLockImpl is platform-specific.
class ScopedPrefsLock {
 public:
  explicit ScopedPrefsLock(std::unique_ptr<ScopedPrefsLockImpl> impl);
  ScopedPrefsLock(const ScopedPrefsLock&) = delete;
  ScopedPrefsLock& operator=(const ScopedPrefsLock&) = delete;
  ~ScopedPrefsLock();

 private:
  std::unique_ptr<ScopedPrefsLockImpl> impl_;
};

// Returns a ScopedPrefsLock, or nullptr if the lock could not be acquired
// within the timeout. While the ScopedPrefsLock exists, no other process on
// the machine may access global prefs.
std::unique_ptr<ScopedPrefsLock> AcquireGlobalPrefsLock(
    base::TimeDelta timeout);

}  // namespace updater

#endif  // CHROME_UPDATER_PREFS_IMPL_H_
