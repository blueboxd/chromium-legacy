// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

ScopedTestSystemNSSKeySlotMixin::ScopedTestSystemNSSKeySlotMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ScopedTestSystemNSSKeySlotMixin::~ScopedTestSystemNSSKeySlotMixin() = default;

PK11SlotInfo* ScopedTestSystemNSSKeySlotMixin::slot() {
  return scoped_test_system_nss_key_slot_->slot();
}

void ScopedTestSystemNSSKeySlotMixin::SetUpOnMainThread() {
  bool system_slot_initialized_successfully = false;
  base::RunLoop loop;
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ScopedTestSystemNSSKeySlotMixin::InitializeOnIo,
                     base::Unretained(this),
                     &system_slot_initialized_successfully),
      loop.QuitClosure());
  loop.Run();
  ASSERT_TRUE(system_slot_initialized_successfully);
}

void ScopedTestSystemNSSKeySlotMixin::TearDownOnMainThread() {
  base::RunLoop loop;
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ScopedTestSystemNSSKeySlotMixin::DestroyOnIo,
                     base::Unretained(this)),
      loop.QuitClosure());
  loop.Run();
}

void ScopedTestSystemNSSKeySlotMixin::InitializeOnIo(bool* out_success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_test_system_nss_key_slot_ =
      std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
  *out_success = scoped_test_system_nss_key_slot_->ConstructedSuccessfully();
}

void ScopedTestSystemNSSKeySlotMixin::DestroyOnIo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_test_system_nss_key_slot_.reset();
}

}  // namespace chromeos
