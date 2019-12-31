// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_

#include <pk11pub.h>
#include <memory>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace crypto {
class ScopedTestSystemNSSKeySlot;
}

namespace chromeos {

// Owns a persistent NSS software database in a temporary directory and the
// association of the system slot with this database.
//
// This mixin performs the blocking initialization/destruction in the
// {SetUp|TearDown}OnMainThread methods.
class ScopedTestSystemNSSKeySlotMixin final : public InProcessBrowserTestMixin {
 public:
  explicit ScopedTestSystemNSSKeySlotMixin(InProcessBrowserTestMixinHost* host);
  ScopedTestSystemNSSKeySlotMixin(const ScopedTestSystemNSSKeySlotMixin&) =
      delete;
  ScopedTestSystemNSSKeySlotMixin& operator=(
      const ScopedTestSystemNSSKeySlotMixin&) = delete;
  ~ScopedTestSystemNSSKeySlotMixin() override;

  crypto::ScopedTestSystemNSSKeySlot* scoped_test_system_nss_key_slot() {
    return scoped_test_system_nss_key_slot_.get();
  }
  const crypto::ScopedTestSystemNSSKeySlot* scoped_test_system_nss_key_slot()
      const {
    return scoped_test_system_nss_key_slot_.get();
  }
  PK11SlotInfo* slot();

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  void InitializeOnIo(bool* out_success);
  void DestroyOnIo();

  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot>
      scoped_test_system_nss_key_slot_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_MIXIN_H_
