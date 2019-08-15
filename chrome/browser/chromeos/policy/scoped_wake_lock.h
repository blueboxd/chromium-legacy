// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SCOPED_WAKE_LOCK_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SCOPED_WAKE_LOCK_H_

#include <string>

#include "base/macros.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace policy {

// Acquires a wake lock of type |type| with reason |reason| upon construction.
// Releases the wake lock upon destruction. Movable only.
class ScopedWakeLock {
 public:
  ScopedWakeLock(service_manager::Connector* connector,
                 device::mojom::WakeLockType type,
                 const std::string& reason);
  ~ScopedWakeLock();

  // Movable only.
  ScopedWakeLock(ScopedWakeLock&& other);
  ScopedWakeLock& operator=(ScopedWakeLock&& other);

 private:
  device::mojom::WakeLockPtr wake_lock_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWakeLock);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SCOPED_WAKE_LOCK_H_
