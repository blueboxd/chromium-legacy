// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"

#include <string>

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::AuthFailure;
using chromeos::KioskAppLaunchError;

namespace {

// Key under "kiosk" dictionary to store the last launch error.
constexpr char kKeyLaunchError[] = "launch_error";

// Key under "kiosk" dictionary to store the last cryptohome error.
constexpr char kKeyCryptohomeFailure[] = "cryptohome_failure";

// Get Kiosk dictionary value. It is replaced after each update.
const base::DictionaryValue* GetKioskDictionary() {
  return g_browser_process->local_state()->GetDictionary(
      chromeos::KioskAppManager::kKioskDictionaryName);
}

}  // namespace

class KioskAppLaunchErrorTest : public testing::Test {
 public:
  KioskAppLaunchErrorTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~KioskAppLaunchErrorTest() override = default;

  // Verify the mapping from the error code to the message.
  void VerifyErrorMessage(KioskAppLaunchError::Error error,
                          const std::string& expected_message) const {
    EXPECT_EQ(KioskAppLaunchError::GetErrorMessage(error), expected_message);
  }

 private:
  ScopedTestingLocalState local_state_;
};

TEST_F(KioskAppLaunchErrorTest, GetErrorMessage) {
  std::string expected_message;
  VerifyErrorMessage(KioskAppLaunchError::NONE, expected_message);

  expected_message = l10n_util::GetStringUTF8(IDS_KIOSK_APP_FAILED_TO_LAUNCH);
  VerifyErrorMessage(KioskAppLaunchError::HAS_PENDING_LAUNCH, expected_message);
  VerifyErrorMessage(KioskAppLaunchError::NOT_KIOSK_ENABLED, expected_message);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_RETRIEVE_HASH,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::POLICY_LOAD_FAILED, expected_message);
  VerifyErrorMessage(KioskAppLaunchError::ARC_AUTH_FAILED, expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_MOUNT);
  VerifyErrorMessage(KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING,
                     expected_message);
  VerifyErrorMessage(KioskAppLaunchError::ALREADY_MOUNTED, expected_message);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_MOUNT, expected_message);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_REMOVE, expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_INSTALL);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_INSTALL, expected_message);

  expected_message = l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_USER_CANCEL);
  VerifyErrorMessage(KioskAppLaunchError::USER_CANCEL, expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_DOWNLOAD);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_DOWNLOAD, expected_message);

  expected_message =
      l10n_util::GetStringUTF8(IDS_KIOSK_APP_ERROR_UNABLE_TO_LAUNCH);
  VerifyErrorMessage(KioskAppLaunchError::UNABLE_TO_LAUNCH, expected_message);
}

TEST_F(KioskAppLaunchErrorTest, SaveError) {
  // No launch error is stored before it is saved.
  EXPECT_FALSE(GetKioskDictionary()->HasKey(kKeyLaunchError));
  KioskAppLaunchError::Save(KioskAppLaunchError::ERROR_COUNT);

  // The launch error can be retrieved.
  int out_error;
  EXPECT_TRUE(GetKioskDictionary()->GetInteger(kKeyLaunchError, &out_error));
  EXPECT_EQ(out_error, KioskAppLaunchError::ERROR_COUNT);
  EXPECT_EQ(KioskAppLaunchError::Get(), KioskAppLaunchError::ERROR_COUNT);

  // The launch error is cleaned up after clear operation.
  KioskAppLaunchError::RecordMetricAndClear();
  EXPECT_FALSE(GetKioskDictionary()->HasKey(kKeyLaunchError));
  EXPECT_EQ(KioskAppLaunchError::Get(), KioskAppLaunchError::NONE);
}

TEST_F(KioskAppLaunchErrorTest, SaveCryptohomeFailure) {
  // No cryptohome failure is stored before it is saved.
  EXPECT_FALSE(GetKioskDictionary()->HasKey(kKeyCryptohomeFailure));
  AuthFailure auth_failure(AuthFailure::FailureReason::AUTH_DISABLED);
  KioskAppLaunchError::SaveCryptohomeFailure(auth_failure);

  // The cryptohome failure can be retrieved.
  int out_error;
  EXPECT_TRUE(
      GetKioskDictionary()->GetInteger(kKeyCryptohomeFailure, &out_error));
  EXPECT_EQ(out_error, auth_failure.reason());

  // The cryptohome failure is cleaned up after clear operation.
  KioskAppLaunchError::RecordMetricAndClear();
  EXPECT_FALSE(GetKioskDictionary()->HasKey(kKeyCryptohomeFailure));
}
