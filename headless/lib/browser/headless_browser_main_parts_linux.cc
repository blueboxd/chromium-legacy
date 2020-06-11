// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "build/build_config.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace headless {

void HeadlessBrowserMainParts::PostMainMessageLoopStart() {
#if defined(USE_DBUS) && !defined(OS_CHROMEOS)
  bluez::BluezDBusManager::Initialize(/*system_bus=*/nullptr);
#endif
}

}  // namespace headless
