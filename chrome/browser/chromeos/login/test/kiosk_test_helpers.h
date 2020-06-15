// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_KIOSK_TEST_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_KIOSK_TEST_HELPERS_H_

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"

namespace chromeos {

// Common classes that can be used for kiosk mode testing.
// Waits for kiosk session to be initialized.
class KioskSessionInitializedWaiter : public KioskAppManagerObserver {
 public:
  KioskSessionInitializedWaiter();
  ~KioskSessionInitializedWaiter() override;
  KioskSessionInitializedWaiter(const KioskSessionInitializedWaiter&) = delete;
  KioskSessionInitializedWaiter& operator=(
      const KioskSessionInitializedWaiter&) = delete;

  void Wait();

 private:
  // KioskAppManagerObserver:
  void OnKioskSessionInitialized() override;

  ScopedObserver<KioskAppManagerBase, KioskAppManagerObserver> scoped_observer_{
      this};
  base::RunLoop run_loop_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_KIOSK_TEST_HELPERS_H_
