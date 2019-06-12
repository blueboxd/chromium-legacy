// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::SaveArg;

namespace extensions {

namespace {

ACTION_P(CaptureArg, wrapper) {
  *wrapper = arg0.Clone();
}

}  // namespace

class SafeBrowsingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit SafeBrowsingEventObserver(const std::string& event_name)
      : event_name_(event_name) {}

  ~SafeBrowsingEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs() { return std::move(event_args_); }

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    if (event.event_name == event_name_) {
      event_args_ = event.event_args->Clone();
    }
  }

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingEventObserver);
};

std::unique_ptr<KeyedService> BuildSafeBrowsingPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      new SafeBrowsingPrivateEventRouter(context));
}

class SafeBrowsingPrivateEventRouterTest : public testing::Test {
 public:
  SafeBrowsingPrivateEventRouterTest() = default;
  ~SafeBrowsingPrivateEventRouterTest() override = default;

  void TriggerOnPolicySpecifiedPasswordReuseDetectedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(GURL("https://phishing.com/"),
                                                 "user_name_1",
                                                 /*is_phishing_url*/ true);
  }

  void TriggerOnPolicySpecifiedPasswordChangedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->OnPolicySpecifiedPasswordChanged("user_name_2");
  }

  void TriggerOnDangerousDownloadOpenedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->OnDangerousDownloadOpened(GURL("https://evil.com/malware.exe"),
                                    "/path/to/malware.exe",
                                    "sha256_or_malware_exe");
  }

  void TriggerOnSecurityInterstitialShownEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->OnSecurityInterstitialShown(GURL("https://phishing.com/"), "PHISHING",
                                      0);
  }

  void TriggerOnSecurityInterstitialProceededEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->OnSecurityInterstitialProceeded(GURL("https://phishing.com/"),
                                          "PHISHING", -201);
  }

  void SetUpRouters() {
    event_router_ = extensions::CreateAndUseTestEventRouter(&profile_);
    SafeBrowsingPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));

    // Make sure real-time feature is eanbled so that the tests will run.
    scoped_feature_list_.InitAndEnableFeature(
        SafeBrowsingPrivateEventRouter::kRealtimeReportingFeature);

    // Set a mock cloud policy client in the router.  The router will own the
    // client, but a pointer to the client is maintained in the test class to
    // manage expectations.
    client_ = new policy::MockCloudPolicyClient();
    std::unique_ptr<policy::CloudPolicyClient> client(client_);
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(&profile_)
        ->SetCloudPolicyClientForTesting(std::move(client));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  extensions::TestEventRouter* event_router_ = nullptr;
  policy::MockCloudPolicyClient* client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouterTest);
};

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value wrapper;
  EXPECT_CALL(*client_, UploadRealtimeReport(_, _))
      .WillOnce(CaptureArg(&wrapper));

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("user_name_1", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("https://phishing.com/",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ("user_name_1", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordChanged) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value wrapper;
  EXPECT_CALL(*client_, UploadRealtimeReport(_, _))
      .WillOnce(CaptureArg(&wrapper));

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("user_name_2", captured_args.GetString());

  Mock::VerifyAndClearExpectations(client_);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("user_name_2", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadOpened) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value wrapper;
  EXPECT_CALL(*client_, UploadRealtimeReport(_, _))
      .WillOnce(CaptureArg(&wrapper));

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://evil.com/malware.exe",
            captured_args.FindKey("url")->GetString());
  EXPECT_EQ("/path/to/malware.exe",
            captured_args.FindKey("fileName")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());
  EXPECT_EQ("sha256_or_malware_exe",
            captured_args.FindKey("downloadDigestSha256")->GetString());

  Mock::VerifyAndClearExpectations(client_);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/malware.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnSecurityInterstitialProceeded) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value wrapper;
  EXPECT_CALL(*client_, UploadRealtimeReport(_, _))
      .WillOnce(CaptureArg(&wrapper));

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_EQ("-201", captured_args.FindKey("netErrorCode")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(-201, *event->FindIntKey(
                      SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_TRUE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSecurityInterstitialShown) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value wrapper;
  EXPECT_CALL(*client_, UploadRealtimeReport(_, _))
      .WillOnce(CaptureArg(&wrapper));

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_FALSE(captured_args.FindKey("netErrorCode"));
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(
      0, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_FALSE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}
}  // namespace extensions
