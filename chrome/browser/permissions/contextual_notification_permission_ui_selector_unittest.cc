// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/crowd_deny.pb.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kTestDomainUnknown[] = "unknown.com";
constexpr char kTestDomainAcceptable[] = "acceptable.com";
constexpr char kTestDomainSpammy[] = "spammy.com";
constexpr char kTestDomainSpammyWarn[] = "warn-spammy.com";
constexpr char kTestDomainAbusivePrompts[] = "abusive_prompts.com";
constexpr char kTestDomainAbusivePromptsWarn[] = "warn_prompts.com";

constexpr char kTestOriginNoData[] = "https://nodata.com/";
constexpr char kTestOriginUnknown[] = "https://unknown.com/";
constexpr char kTestOriginAcceptable[] = "https://acceptable.com/";
constexpr char kTestOriginSpammy[] = "https://spammy.com/";
constexpr char kTestOriginSpammyWarn[] = "https://warn-spammy.com/";
constexpr char kTestOriginAbusivePrompts[] = "https://abusive-prompts.com/";
constexpr char kTestOriginSubDomainOfAbusivePrompts[] =
    "https://b.abusive-prompts.com/";
constexpr char kTestOriginAbusivePromptsWarn[] =
    "https://warn-abusive-prompts.com/";

constexpr const char* kAllTestingOrigins[] = {
    kTestOriginNoData,
    kTestOriginUnknown,
    kTestOriginAcceptable,
    kTestOriginSpammy,
    kTestOriginSpammyWarn,
    kTestOriginAbusivePrompts,
    kTestOriginSubDomainOfAbusivePrompts,
    kTestOriginAbusivePromptsWarn,
};

}  // namespace

class ContextualNotificationPermissionUiSelectorTest : public testing::Test {
 public:
  using UiToUse = ContextualNotificationPermissionUiSelector::UiToUse;
  using QuietUiReason =
      ContextualNotificationPermissionUiSelector::QuietUiReason;
  using SiteReputation = chrome_browser_crowd_deny::SiteReputation;

  ContextualNotificationPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()),
        contextual_selector_(testing_profile_.get()) {}
  ~ContextualNotificationPermissionUiSelectorTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();

    fake_database_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    testing::Test::TearDown();
  }

  void SetQuietUiEnabledInPrefs(bool enabled) {
    testing_profile_->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, enabled);
  }

  void LoadTestPreloadData() {
    using SiteReputation = CrowdDenyPreloadData::SiteReputation;

    SiteReputation reputation_unknown;
    reputation_unknown.set_domain(kTestDomainUnknown);
    reputation_unknown.set_notification_ux_quality(SiteReputation::UNKNOWN);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginUnknown)),
        std::move(reputation_unknown));

    SiteReputation reputation_acceptable;
    reputation_acceptable.set_domain(kTestDomainAcceptable);
    reputation_acceptable.set_notification_ux_quality(
        SiteReputation::ACCEPTABLE);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAcceptable)),
        std::move(reputation_acceptable));

    SiteReputation reputation_spammy;
    reputation_spammy.set_domain(kTestDomainSpammy);
    reputation_spammy.set_notification_ux_quality(
        SiteReputation::UNSOLICITED_PROMPTS);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginSpammy)),
        std::move(reputation_spammy));

    SiteReputation reputation_spammy_warn;
    reputation_spammy_warn.set_domain(kTestDomainSpammyWarn);
    reputation_spammy_warn.set_notification_ux_quality(
        SiteReputation::UNSOLICITED_PROMPTS);
    reputation_spammy_warn.set_warning_only(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginSpammyWarn)),
        std::move(reputation_spammy_warn));

    SiteReputation reputation_abusive;
    reputation_abusive.set_domain(kTestDomainAbusivePrompts);
    reputation_abusive.set_notification_ux_quality(
        SiteReputation::ABUSIVE_PROMPTS);
    reputation_abusive.set_include_subdomains(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusivePrompts)),
        std::move(reputation_abusive));

    SiteReputation reputation_abusive_warn;
    reputation_abusive_warn.set_domain(kTestDomainAbusivePromptsWarn);
    reputation_abusive_warn.set_notification_ux_quality(
        SiteReputation::ABUSIVE_PROMPTS);
    reputation_abusive_warn.set_warning_only(true);
    reputation_abusive_warn.set_include_subdomains(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusivePromptsWarn)),
        std::move(reputation_abusive_warn));
  }

  void AddUrlToFakeApiAbuseBlocklist(const GURL& url) {
    safe_browsing::ThreatMetadata test_metadata;
    test_metadata.api_permissions.emplace("NOTIFICATIONS");
    fake_database_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

  void LoadTestSafeBrowsingBlocklist() {
    // For simplicity, Safe Browsing will simulate a match for all testing
    // origins, tests can clear the fake results to simulate a miss.
    for (const char* origin : kAllTestingOrigins) {
      AddUrlToFakeApiAbuseBlocklist(GURL(origin));
    }
  }

  void ClearSafeBrowsingBlocklist() {
    fake_database_manager_->RemoveAllBlacklistedUrls();
  }

  void QueryAndExpectDecisionForUrl(
      const GURL& origin,
      UiToUse ui_to_use,
      base::Optional<QuietUiReason> quiet_ui_reason) {
    permissions::MockPermissionRequest mock_request(
        std::string(),
        permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS, origin);
    base::MockCallback<
        ContextualNotificationPermissionUiSelector::DecisionMadeCallback>
        mock_callback;
    EXPECT_CALL(mock_callback, Run(ui_to_use, quiet_ui_reason));
    contextual_selector_.SelectUiToUse(&mock_request, mock_callback.Get());
    task_environment_.RunUntilIdle();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  testing::ScopedCrowdDenyPreloadDataOverride testing_preload_data_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;

  ContextualNotificationPermissionUiSelector contextual_selector_;

  DISALLOW_COPY_AND_ASSIGN(ContextualNotificationPermissionUiSelectorTest);
};

// With all the field trials enabled, test all combinations of:
//   (a) quiet UI being enabled/disabled in prefs, and
//   (b) positive/negative Safe Browsing verdicts.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       PrefAndSafeBrowsingCombinations) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts,
      {{Config::kEnableAdaptiveActivation, "true"},
       {Config::kEnableCrowdDenyTriggering, "true"},
       {Config::kEnableAbusiveRequestBlocking, "true"},
       {Config::kEnableAbusiveRequestWarning, "true"}});

  LoadTestPreloadData();

  {
    SetQuietUiEnabledInPrefs(false);
    ClearSafeBrowsingBlocklist();

    SCOPED_TRACE("Quiet UI disabled in prefs, Safe Browsing verdicts negative");
    for (const auto* origin_string : kAllTestingOrigins) {
      SCOPED_TRACE(origin_string);
      QueryAndExpectDecisionForUrl(GURL(origin_string), UiToUse::kNormalUi,
                                   base::nullopt);
    }
  }

  {
    SetQuietUiEnabledInPrefs(true);
    ClearSafeBrowsingBlocklist();

    SCOPED_TRACE("Quiet UI enabled in prefs, Safe Browsing verdicts negative");
    for (const auto* origin_string : kAllTestingOrigins) {
      SCOPED_TRACE(origin_string);
      QueryAndExpectDecisionForUrl(GURL(origin_string), UiToUse::kQuietUi,
                                   QuietUiReason::kEnabledInPrefs);
    }
  }

  {
    SetQuietUiEnabledInPrefs(false);
    LoadTestSafeBrowsingBlocklist();

    const struct {
      const char* origin_string;
      UiToUse expected_ui;
      base::Optional<QuietUiReason> expected_ui_reason;
    } kTestCases[] = {
        {kTestOriginNoData, UiToUse::kNormalUi, base::nullopt},
        {kTestOriginUnknown, UiToUse::kNormalUi, base::nullopt},
        {kTestOriginAcceptable, UiToUse::kNormalUi, base::nullopt},
        {kTestOriginSpammy, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredByCrowdDeny},
        {kTestOriginSpammyWarn, UiToUse::kNormalUi, base::nullopt},
        {kTestOriginAbusivePrompts, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginSubDomainOfAbusivePrompts, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginAbusivePromptsWarn, UiToUse::kNormalUi, base::nullopt},
    };

    SCOPED_TRACE("Quiet UI disabled in prefs, Safe Browsing verdicts positive");
    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_string);
      QueryAndExpectDecisionForUrl(GURL(test.origin_string), test.expected_ui,
                                   test.expected_ui_reason);
    }
  }

  {
    SetQuietUiEnabledInPrefs(true);
    LoadTestSafeBrowsingBlocklist();

    const struct {
      const char* origin_string;
      UiToUse expected_ui;
      base::Optional<QuietUiReason> expected_ui_reason;
    } kTestCases[] = {
        {kTestOriginNoData, UiToUse::kQuietUi, QuietUiReason::kEnabledInPrefs},
        {kTestOriginUnknown, UiToUse::kQuietUi, QuietUiReason::kEnabledInPrefs},
        {kTestOriginAcceptable, UiToUse::kQuietUi,
         QuietUiReason::kEnabledInPrefs},
        {kTestOriginSpammy, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredByCrowdDeny},
        {kTestOriginSpammyWarn, UiToUse::kQuietUi,
         QuietUiReason::kEnabledInPrefs},
        {kTestOriginAbusivePrompts, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginSubDomainOfAbusivePrompts, UiToUse::kQuietUi,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginAbusivePromptsWarn, UiToUse::kQuietUi,
         QuietUiReason::kEnabledInPrefs},
    };

    SCOPED_TRACE("Quiet UI enabled in prefs, Safe Browsing verdicts positive");
    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_string);
      QueryAndExpectDecisionForUrl(GURL(test.origin_string), test.expected_ui,
                                   test.expected_ui_reason);
    }
  }
}

TEST_F(ContextualNotificationPermissionUiSelectorTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kQuietNotificationPrompts);

  SetQuietUiEnabledInPrefs(true);
  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto* origin_string : kAllTestingOrigins) {
    SCOPED_TRACE(origin_string);
    QueryAndExpectDecisionForUrl(GURL(origin_string), UiToUse::kNormalUi,
                                 base::nullopt);
  }
}

// The feature is enabled but no adaptive triggers are enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest, AllTriggersDisabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts,
      {{Config::kEnableAdaptiveActivation, "true"}});

  SetQuietUiEnabledInPrefs(true);
  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto* origin_string : kAllTestingOrigins) {
    SCOPED_TRACE(origin_string);
    QueryAndExpectDecisionForUrl(GURL(origin_string), UiToUse::kQuietUi,
                                 QuietUiReason::kEnabledInPrefs);
  }
}

// The feature is enabled but only the `crowd deny` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest, OnlyCrowdDenyEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts,
      {{Config::kEnableAdaptiveActivation, "true"},
       {Config::kEnableCrowdDenyTriggering, "true"}});

  SetQuietUiEnabledInPrefs(false);
  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    UiToUse expected_ui;
    base::Optional<QuietUiReason> expected_ui_reason;
  } kTestCases[] = {
      {kTestOriginSpammy, UiToUse::kQuietUi,
       QuietUiReason::kTriggeredByCrowdDeny},
      {kTestOriginSpammyWarn, UiToUse::kNormalUi, base::nullopt},
      {kTestOriginAbusivePrompts, UiToUse::kNormalUi, base::nullopt},
      {kTestOriginSubDomainOfAbusivePrompts, UiToUse::kNormalUi, base::nullopt},
      {kTestOriginAbusivePromptsWarn, UiToUse::kNormalUi, base::nullopt},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string), test.expected_ui,
                                 test.expected_ui_reason);
  }
}

// The feature is enabled but only the `abusive prompts` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyAbusivePromptsEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts,
      {{Config::kEnableAdaptiveActivation, "true"},
       {Config::kEnableAbusiveRequestBlocking, "true"}});

  SetQuietUiEnabledInPrefs(false);
  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    UiToUse expected_ui;
    base::Optional<QuietUiReason> expected_ui_reason;
  } kTestCases[] = {
      {kTestOriginSpammy, UiToUse::kNormalUi, base::nullopt},
      {kTestOriginSpammyWarn, UiToUse::kNormalUi, base::nullopt},
      {kTestOriginAbusivePrompts, UiToUse::kQuietUi,
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      {kTestOriginSubDomainOfAbusivePrompts, UiToUse::kQuietUi,
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      {kTestOriginAbusivePromptsWarn, UiToUse::kNormalUi, base::nullopt},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string), test.expected_ui,
                                 test.expected_ui_reason);
  }
}

TEST_F(ContextualNotificationPermissionUiSelectorTest,
       CrowdDenyHoldbackChance) {
  const struct {
    std::string holdback_chance;
    bool enabled_in_prefs;
    UiToUse expected_ui;
    base::Optional<QuietUiReason> expected_ui_reason;
    bool expected_histogram_bucket;
  } kTestCases[] = {
      // 100% chance to holdback, the UI used should be the normal UI.
      {"1.0", false, UiToUse::kNormalUi, base::nullopt, true},
      // 0% chance to holdback, the UI used should be the quiet UI.
      {"0.0", false, UiToUse::kQuietUi, QuietUiReason::kTriggeredByCrowdDeny},
      // 100% chance to holdback but the quiet UI is enabled by the user in
      // prefs, the UI used should be the quiet UI.
      {"1.0", true, UiToUse::kQuietUi, QuietUiReason::kEnabledInPrefs, true},
  };

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.holdback_chance);
    SCOPED_TRACE(test.enabled_in_prefs);

    using Config = QuietNotificationPermissionUiConfig;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableAbusiveRequestBlocking, "true"},
         {Config::kEnableCrowdDenyTriggering, "true"},
         {Config::kCrowdDenyHoldBackChance, test.holdback_chance}});

    SetQuietUiEnabledInPrefs(test.enabled_in_prefs);

    base::HistogramTester histograms;
    QueryAndExpectDecisionForUrl(GURL(kTestOriginSpammy), test.expected_ui,
                                 test.expected_ui_reason);

    // The hold-back should not apply to other per-site triggers.
    QueryAndExpectDecisionForUrl(GURL(kTestOriginAbusivePrompts),
                                 UiToUse::kQuietUi,
                                 QuietUiReason::kTriggeredDueToAbusiveRequests);

    auto expected_bucket = static_cast<base::HistogramBase::Sample>(
        test.expected_histogram_bucket);
    histograms.ExpectBucketCount("Permissions.CrowdDeny.DidHoldbackQuietUi",
                                 expected_bucket, 1);
  }
}
