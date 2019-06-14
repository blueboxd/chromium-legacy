// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  bool IsReady() override { return true; }
  bool HasValidTargetDevice() override { return true; }
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() = default;
  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfUtilTest() = default;
  ~SendTabToSelfUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    incognito_profile_ = profile()->GetOffTheRecordProfile();
    url_ = GURL("https://www.google.com");
    title_ = base::UTF8ToUTF16(base::StringPiece("Google"));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* incognito_profile_;
  GURL url_;
  base::string16 title_;
};

TEST_F(SendTabToSelfUtilTest, AreFlagsEnabled_True) {
  scoped_feature_list_.InitWithFeatures(
      {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI}, {});

  EXPECT_TRUE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, AreFlagsEnabled_False) {
  scoped_feature_list_.InitWithFeatures(
      {}, {switches::kSyncSendTabToSelf, kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsReceivingEnabled_True) {
  scoped_feature_list_.InitWithFeatures({switches::kSyncSendTabToSelf},
                                        {kSendTabToSelfShowSendingUI});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_TRUE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsOnlySendingEnabled_False) {
  scoped_feature_list_.InitWithFeatures({kSendTabToSelfShowSendingUI},
                                        {switches::kSyncSendTabToSelf});

  EXPECT_FALSE(IsSendingEnabled());
  EXPECT_FALSE(IsReceivingEnabled());
}

TEST_F(SendTabToSelfUtilTest, HasValidTargetDevice) {
  EXPECT_FALSE(HasValidTargetDevice(profile()));

  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

  EXPECT_TRUE(HasValidTargetDevice(profile()));
}

TEST_F(SendTabToSelfUtilTest, ContentRequirementsMet) {
  EXPECT_TRUE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  url_ = GURL("192.168.0.0");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NativePage) {
  url_ = GURL("chrome://flags");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  EXPECT_FALSE(IsContentRequirementsMet(url_, incognito_profile_));
}

}  // namespace

}  // namespace send_tab_to_self
