// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/rsu/lookup_key_uploader.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/mock_enrollment_certificate_uploader.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::attestation::MockEnrollmentCertificateUploader;
using testing::_;
using testing::Invoke;

namespace policy {

namespace {
const char kValidRsuDeviceId[] = "123";
}
class LookupKeyUploaderTest : public chromeos::DeviceSettingsTestBase {
 protected:
  LookupKeyUploaderTest() = default;

  void SetUp() override {
    chromeos::DeviceSettingsTestBase::SetUp();
    pref_service_.registry()->RegisterStringPref(
        prefs::kLastRsuDeviceIdUploaded, std::string());
    lookup_key_uploader_ = std::make_unique<LookupKeyUploader>(
        nullptr, &pref_service_, &certificate_uploader_);
    lookup_key_uploader_->SetClock(&clock_);
    // We initialize clock to imitate real time.
    clock_.Advance(base::TimeDelta::FromDays(50));
  }

  void ExpectSavedIdToBe(const std::string& key) {
    EXPECT_EQ(pref_service_.GetString(prefs::kLastRsuDeviceIdUploaded), key);
  }
  bool NeedsUpload() { return lookup_key_uploader_->needs_upload_; }

  void SetCryptohomeReplyTo(const std::string& rsu_device_id) {
    chromeos::FakeCryptohomeClient::Get()->set_rsu_device_id(rsu_device_id);
  }

  void AdvanceTime() { clock_.Advance(lookup_key_uploader_->kRetryFrequency); }
  void Start() {
    lookup_key_uploader_->OnStoreLoaded(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock clock_;
  MockEnrollmentCertificateUploader certificate_uploader_;
  std::unique_ptr<LookupKeyUploader> lookup_key_uploader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LookupKeyUploaderTest);
};

TEST_F(LookupKeyUploaderTest, Uploads) {
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
      .WillOnce(Invoke([](base::OnceCallback<void(bool status)> callback) {
        std::move(callback).Run(true);
      }));
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  Start();
  ExpectSavedIdToBe(kValidRsuDeviceId);
}

TEST_F(LookupKeyUploaderTest, ReuploadsOnFail) {
  SetCryptohomeReplyTo("");
  Start();
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_)).Times(0);
  EXPECT_TRUE(NeedsUpload());
}

TEST_F(LookupKeyUploaderTest, DoesntUploadTwice) {
  pref_service_.SetString(prefs::kLastRsuDeviceIdUploaded, kValidRsuDeviceId);
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  Start();
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_)).Times(0);
  ExpectSavedIdToBe(kValidRsuDeviceId);
  EXPECT_FALSE(NeedsUpload());
}

TEST_F(LookupKeyUploaderTest, DoesNotUploadVeryFrequently) {
  SetCryptohomeReplyTo("");
  Start();
  EXPECT_TRUE(NeedsUpload());  // Will ask for restart.

  // Next upload should not be executed -- because of the frequency limit.
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  Start();
  ExpectSavedIdToBe("");
  EXPECT_TRUE(NeedsUpload());  // Will ask for restart.

  AdvanceTime();

  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
      .WillOnce(Invoke([](base::OnceCallback<void(bool status)> callback) {
        std::move(callback).Run(true);
      }));
  Start();
  ExpectSavedIdToBe(kValidRsuDeviceId);
  EXPECT_FALSE(NeedsUpload());
}

}  // namespace policy
