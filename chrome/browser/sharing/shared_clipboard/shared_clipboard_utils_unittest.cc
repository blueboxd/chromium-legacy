// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char kEmptyText[] = "";
const char kText[] = "Some text to copy to phone device.";

class MockSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  MockSharingDeviceRegistration()
      : SharingDeviceRegistration(/* pref_service_= */ nullptr,
                                  /* sharing_sync_preference_= */ nullptr,
                                  /* instance_id_driver_= */ nullptr,
                                  /* vapid_key_manager_= */ nullptr) {}

  ~MockSharingDeviceRegistration() override = default;

  MOCK_CONST_METHOD0(IsSharedClipboardSupported, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingDeviceRegistration);
};

class SharedClipboardUtilsTest : public testing::Test {
 public:
  SharedClipboardUtilsTest() = default;

  ~SharedClipboardUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&SharedClipboardUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    if (!create_service_)
      return nullptr;

    return std::make_unique<SharingService>(
        /* sync_prefs= */ nullptr,
        /* vapid_key_manager= */ nullptr,
        std::make_unique<MockSharingDeviceRegistration>(),
        /* fcm_sender= */ nullptr,
        std::make_unique<SharingFCMHandler>(nullptr, nullptr, nullptr),
        /* gcm_driver= */ nullptr,
        /* device_info_tracker= */ nullptr,
        /* local_device_info_provider= */ nullptr,
        /* sync_service */ nullptr,
        /* notification_display_service= */ nullptr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  bool create_service_ = true;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardUtilsTest);
};

}  // namespace

TEST_F(SharedClipboardUtilsTest, UIFlagDisabled_DoNotShowMenu) {
  scoped_feature_list_.InitAndDisableFeature(kSharedClipboardUI);
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, IncognitoProfile_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_FALSE(ShouldOfferSharedClipboard(profile_.GetOffTheRecordProfile(),
                                          base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, EmptyClipboardProtocol_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kEmptyText)));
}

TEST_F(SharedClipboardUtilsTest, ClipboardProtocol_ShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  EXPECT_TRUE(ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, NoSharingService_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  create_service_ = false;
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}

TEST_F(SharedClipboardUtilsTest, EnterprisePolicy_Disabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  // Set the enterprise policy to false:
  profile_.GetPrefs()->SetBoolean(prefs::kSharedClipboardEnabled, false);
  EXPECT_FALSE(
      ShouldOfferSharedClipboard(&profile_, base::ASCIIToUTF16(kText)));
}
