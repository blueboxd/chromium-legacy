// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safe_browsing_handler.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace settings {

class SafeBrowsingHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    handler_ = std::make_unique<SafeBrowsingHandler>(profile());
    handler()->set_web_ui(web_ui());
    handler()->AllowJavascript();
    web_ui()->ClearTrackedCalls();
  }

  TestingProfile* profile() { return &profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }
  SafeBrowsingHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<SafeBrowsingHandler> handler_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
};

// All of the possible managed states for a boolean preference that can be
// both enforced and recommended.
// TODO(crbug.com/1063265): Remove duplication with site_settings_helper.
enum class PrefSetting {
  kEnforcedOff,
  kEnforcedOn,
  kRecommendedOff,
  kRecommendedOn,
  kNotSet,
};

// Possible preference sources supported by TestingPrefService.
// TODO(crbug.com/1063265): Remove duplication with site_settings_helper.
enum class PrefSource {
  kExtension,
  kDevicePolicy,
  kRecommended,
  kNone,
};

void AssertRadioManagedStateEqual(const SafeBrowsingRadioManagedState& a,
                                  const SafeBrowsingRadioManagedState& b) {
  ASSERT_EQ(a.enhanced.disabled, b.enhanced.disabled);
  ASSERT_EQ(a.enhanced.indicator, b.enhanced.indicator);
  ASSERT_EQ(a.standard.disabled, b.standard.disabled);
  ASSERT_EQ(a.standard.indicator, b.standard.indicator);
  ASSERT_EQ(a.disabled.disabled, b.disabled.disabled);
  ASSERT_EQ(a.disabled.indicator, b.disabled.indicator);
}

struct RadioManagedStateTestCase {
  PrefSetting safe_browsing_enhanced;
  PrefSetting safe_browsing_enabled;
  PrefSetting safe_browsing_reporting;
  PrefSource preference_source;
  SafeBrowsingRadioManagedState expected_result;
};

const std::vector<RadioManagedStateTestCase> test_cases = {
    {PrefSetting::kNotSet,
     PrefSetting::kNotSet,
     PrefSetting::kNotSet,
     PrefSource::kNone,
     {{false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kNone}}},
    {PrefSetting::kEnforcedOn,
     PrefSetting::kEnforcedOn,
     PrefSetting::kNotSet,
     PrefSource::kExtension,
     {{true, site_settings::PolicyIndicatorType::kExtension},
      {true, site_settings::PolicyIndicatorType::kExtension},
      {true, site_settings::PolicyIndicatorType::kExtension}}},
    {PrefSetting::kEnforcedOff,
     PrefSetting::kEnforcedOff,
     PrefSetting::kNotSet,
     PrefSource::kDevicePolicy,
     {{true, site_settings::PolicyIndicatorType::kDevicePolicy},
      {true, site_settings::PolicyIndicatorType::kDevicePolicy},
      {true, site_settings::PolicyIndicatorType::kDevicePolicy}}},
    {PrefSetting::kEnforcedOff,
     PrefSetting::kEnforcedOn,
     PrefSetting::kNotSet,
     PrefSource::kExtension,
     {{true, site_settings::PolicyIndicatorType::kExtension},
      {true, site_settings::PolicyIndicatorType::kExtension},
      {true, site_settings::PolicyIndicatorType::kExtension}}},
    {PrefSetting::kRecommendedOn,
     PrefSetting::kRecommendedOn,
     PrefSetting::kNotSet,
     PrefSource::kRecommended,
     {{false, site_settings::PolicyIndicatorType::kRecommended},
      {false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kNone}}},
    {PrefSetting::kRecommendedOff,
     PrefSetting::kRecommendedOn,
     PrefSetting::kNotSet,
     PrefSource::kRecommended,
     {{false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kRecommended},
      {false, site_settings::PolicyIndicatorType::kNone}}},
    {PrefSetting::kRecommendedOff,
     PrefSetting::kRecommendedOff,
     PrefSetting::kNotSet,
     PrefSource::kRecommended,
     {{false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kRecommended}}},
    {PrefSetting::kNotSet,
     PrefSetting::kNotSet,
     PrefSetting::kEnforcedOff,
     PrefSource::kDevicePolicy,
     {{true, site_settings::PolicyIndicatorType::kDevicePolicy},
      {false, site_settings::PolicyIndicatorType::kNone},
      {false, site_settings::PolicyIndicatorType::kNone}}}};

void SetupTestConditions(TestingProfile* profile,
                         const RadioManagedStateTestCase& test_case) {
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile->GetTestingPrefService();
  const std::map<const char*, PrefSetting> pref_to_setting = {
      {prefs::kSafeBrowsingEnhanced, test_case.safe_browsing_enhanced},
      {prefs::kSafeBrowsingEnabled, test_case.safe_browsing_enabled},
      {prefs::kSafeBrowsingScoutReportingEnabled,
       test_case.safe_browsing_reporting}};

  for (const auto& pref_setting : pref_to_setting) {
    if (pref_setting.second == PrefSetting::kNotSet) {
      continue;
    }
    auto pref_value = std::make_unique<base::Value>(
        pref_setting.second == PrefSetting::kRecommendedOn ||
        pref_setting.second == PrefSetting::kEnforcedOn);
    if (test_case.preference_source == PrefSource::kExtension) {
      prefs->SetExtensionPref(pref_setting.first, std::move(pref_value));
    } else if (test_case.preference_source == PrefSource::kDevicePolicy) {
      prefs->SetManagedPref(pref_setting.first, std::move(pref_value));
    } else if (test_case.preference_source == PrefSource::kRecommended) {
      prefs->SetRecommendedPref(pref_setting.first, std::move(pref_value));
    }
  }
}

TEST_F(SafeBrowsingHandlerTest, GenerateRadioManagedState) {
  int count = 0;
  for (const auto& test_case : test_cases) {
    TestingProfile profile;
    SCOPED_TRACE(base::StringPrintf("Test case %d", count++));
    SetupTestConditions(&profile, test_case);
    AssertRadioManagedStateEqual(
        handler()->GetSafeBrowsingRadioManagedState(&profile),
        test_case.expected_result);
  }
}

TEST_F(SafeBrowsingHandlerTest, ProvideRadioManagedState) {
  // Test that the handler correctly wraps the generated result.
  const std::string kNone = "none";
  const std::string kDevicePolicy = "devicePolicy";
  const std::string kCallbackId = "callback";
  const std::vector<std::string> kRadioNames = {"enhanced", "standard",
                                                "disabled"};

  // Check that the default radio state is handled correctly.
  base::ListValue get_args;
  get_args.AppendString(kCallbackId);
  handler()->HandleGetSafeBrowsingRadioManagedState(&get_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    for (const auto& control_name : kRadioNames) {
      auto* control_state = data.arg3()->FindPath(control_name);
      ASSERT_FALSE(control_state->FindKey("disabled")->GetBool());
      ASSERT_EQ(kNone, control_state->FindKey("indicator")->GetString());
    }
  }

  // Create a fully managed state and check it is returned correctly.
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();
  pref_service->SetManagedPref(prefs::kSafeBrowsingEnhanced,
                               std::make_unique<base::Value>(true));
  pref_service->SetManagedPref(prefs::kSafeBrowsingEnabled,
                               std::make_unique<base::Value>(true));

  handler()->HandleGetSafeBrowsingRadioManagedState(&get_args);
  {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->GetBool());
    for (const auto& control_name : kRadioNames) {
      auto* control_state = data.arg3()->FindPath(control_name);
      ASSERT_TRUE(control_state->FindKey("disabled")->GetBool());
      ASSERT_EQ(kDevicePolicy,
                control_state->FindKey("indicator")->GetString());
    }
  }
}

TEST_F(SafeBrowsingHandlerTest, ValidateSafeBrowsingPrefs) {
  base::ListValue args;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  pref_service->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  // Ensure the preference is not changed when the Enhanced feature is enabled.
  scoped_feature_list_.InitWithFeatures({safe_browsing::kEnhancedProtection},
                                        {});
  handler()->HandleValidateSafeBrowsingEnhanced(&args);
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced));
  scoped_feature_list_.Reset();

  // Ensure the preference is disabled when the Enhanced feature is disabled.
  scoped_feature_list_.InitWithFeatures({},
                                        {safe_browsing::kEnhancedProtection});
  handler()->HandleValidateSafeBrowsingEnhanced(&args);
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kSafeBrowsingEnhanced));
}

}  // namespace settings
