// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_CORE_SCHEDULING_H_
#define CHROMEOS_SYSTEM_CORE_SCHEDULING_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace system {

// EnableCoreScheduVlingIfAvailable will turn on core scheduling for a process
// if it's available,
void CHROMEOS_EXPORT EnableCoreSchedulingIfAvailable();

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_CORE_SCHEDULING_H_
