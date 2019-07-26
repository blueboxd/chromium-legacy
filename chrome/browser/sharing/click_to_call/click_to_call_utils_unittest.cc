// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using SharingMessage = chrome_browser_sharing::SharingMessage;

namespace {

const char kEmptyTelUrl[] = "tel:";
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";

class MockSharingService : public SharingService {
 public:
  explicit MockSharingService(std::unique_ptr<SharingFCMHandler> fcm_handler)
      : SharingService(/* sync_prefs= */ nullptr,
                       /* vapid_key_manager= */ nullptr,
                       /* sharing_device_registration= */ nullptr,
                       /* fcm_sender= */ nullptr,
                       std::move(fcm_handler),
                       /* gcm_driver= */ nullptr,
                       /* device_info_tracker= */ nullptr,
                       /* local_device_info_provider= */ nullptr,
                       /* sync_service */ nullptr) {}

  ~MockSharingService() override = default;

  MOCK_CONST_METHOD0(GetState, State());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingService);
};

class ClickToCallUtilsTest : public testing::Test {
 public:
  ClickToCallUtilsTest() = default;

  ~ClickToCallUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&ClickToCallUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    if (!create_service_)
      return nullptr;

    return std::make_unique<NiceMock<MockSharingService>>(
        std::make_unique<SharingFCMHandler>(nullptr, nullptr));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  bool create_service_ = true;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallUtilsTest);
};

}  // namespace

TEST_F(ClickToCallUtilsTest, NonTelProtocol_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCall(&profile_, GURL(kNonTelUrl)));
}

TEST_F(ClickToCallUtilsTest, UIFlagDisabled_DoNotShowMenu) {
  scoped_feature_list_.InitAndDisableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCall(&profile_, GURL(kTelUrl)));
}

TEST_F(ClickToCallUtilsTest, IncognitoProfile_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(
      ShouldOfferClickToCall(profile_.GetOffTheRecordProfile(), GURL(kTelUrl)));
}

TEST_F(ClickToCallUtilsTest, EmptyTelProtocol_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_FALSE(ShouldOfferClickToCall(&profile_, GURL(kEmptyTelUrl)));
}

TEST_F(ClickToCallUtilsTest, TelProtocol_ShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  EXPECT_TRUE(ShouldOfferClickToCall(&profile_, GURL(kTelUrl)));
}

TEST_F(ClickToCallUtilsTest, NoSharingService_DoNotShowMenu) {
  scoped_feature_list_.InitAndEnableFeature(kClickToCallUI);
  create_service_ = false;
  EXPECT_FALSE(ShouldOfferClickToCall(&profile_, GURL(kTelUrl)));
}
