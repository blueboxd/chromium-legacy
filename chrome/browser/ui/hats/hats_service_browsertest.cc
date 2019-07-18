// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"

namespace {

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetMetricsConsent);
};

class HatsServiceBrowserTestBase : public InProcessBrowserTest {
 protected:
  HatsServiceBrowserTestBase()
      : on_hats_dialog_show_(
            base::BindRepeating(&HatsServiceBrowserTestBase::OnHatsDialogShow,
                                base::Unretained(this))) {}
  ~HatsServiceBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    base::AddActionCallback(on_hats_dialog_show_);
  }

  void TearDownOnMainThread() override {
    base::RemoveActionCallback(on_hats_dialog_show_);
  }

  HatsService* GetHatsService() {
    return HatsServiceFactory::GetForProfile(browser()->profile(), true);
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  bool HatsDialogShowRequested() { return hats_dialog_show_requested_; }

 private:
  void OnHatsDialogShow(const std::string& action) {
    if (action == "HatsBubble.Show") {
      ASSERT_FALSE(hats_dialog_show_requested_);
      hats_dialog_show_requested_ = true;
    }
  }

  bool hats_dialog_show_requested_ = false;
  base::ActionCallback on_hats_dialog_show_;
  base::Optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceBrowserTestBase);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceBrowserTestBase, DialogNotShownOnDefault) {
  GetHatsService()->LaunchSatisfactionSurvey();
  EXPECT_FALSE(HatsDialogShowRequested());
}

namespace {

class HatsServiceProbabilityZero : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityZero() = default;
  ~HatsServiceProbabilityZero() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // The HatsService must not be created so that feature parameters can be
    // injected below.
    ASSERT_FALSE(
        HatsServiceFactory::GetForProfile(browser()->profile(), false));
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kHappinessTrackingSurveysForDesktop,
        {{"probability", "0.000"}});
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityZero);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityZero, NoShow) {
  GetHatsService()->LaunchSatisfactionSurvey();
  EXPECT_FALSE(HatsDialogShowRequested());
}

namespace {

class HatsServiceProbabilityOne : public HatsServiceBrowserTestBase {
 protected:
  HatsServiceProbabilityOne() = default;
  ~HatsServiceProbabilityOne() override = default;

 private:
  void SetUpOnMainThread() override {
    HatsServiceBrowserTestBase::SetUpOnMainThread();

    // The HatsService must not be created so that feature parameters can be
    // injected below.
    ASSERT_FALSE(
        HatsServiceFactory::GetForProfile(browser()->profile(), false));
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kHappinessTrackingSurveysForDesktop,
        {{"probability", "1.000"}});
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HatsServiceProbabilityOne);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, NoShowConsentNotGiven) {
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSatisfactionSurvey();
  EXPECT_FALSE(HatsDialogShowRequested());
}

IN_PROC_BROWSER_TEST_F(HatsServiceProbabilityOne, AlwaysShow) {
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  GetHatsService()->LaunchSatisfactionSurvey();
  EXPECT_TRUE(HatsDialogShowRequested());
}
