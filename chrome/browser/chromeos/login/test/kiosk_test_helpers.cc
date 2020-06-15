// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/kiosk_test_helpers.h"

namespace chromeos {

KioskSessionInitializedWaiter::KioskSessionInitializedWaiter() {
  scoped_observer_.Add(KioskAppManager::Get());
}

KioskSessionInitializedWaiter::~KioskSessionInitializedWaiter() = default;

void KioskSessionInitializedWaiter::Wait() {
  if (KioskAppManager::Get()->app_session())
    return;

  run_loop_.Run();
}

void KioskSessionInitializedWaiter::OnKioskSessionInitialized() {
  run_loop_.Quit();
}

}  // namespace chromeos
