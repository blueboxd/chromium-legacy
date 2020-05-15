// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_cookie_prefs.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;

namespace content_settings {

class GeneratedCookiePrefsTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GeneratedCookiePrefsTest, SessionOnlyPref) {
  auto pref =
      std::make_unique<content_settings::GeneratedCookieSessionOnlyPref>(
          profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Ensure an allow content setting sets the preference to false and enabled.
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  auto pref_object = pref->GetPrefObject();
  EXPECT_FALSE(pref_object->value->GetBool());
  EXPECT_FALSE(*pref_object->user_control_disabled);

  // Ensure setting the preference correctly updates content settings and the
  // preference state.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      map->GetDefaultContentSetting(ContentSettingsType::COOKIES, nullptr),
      ContentSetting::CONTENT_SETTING_SESSION_ONLY);
  pref_object = pref->GetPrefObject();
  EXPECT_TRUE(pref_object->value->GetBool());
  EXPECT_FALSE(*pref_object->user_control_disabled);

  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(false).get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      map->GetDefaultContentSetting(ContentSettingsType::COOKIES, nullptr),
      ContentSetting::CONTENT_SETTING_ALLOW);
  pref_object = pref->GetPrefObject();
  EXPECT_FALSE(pref_object->value->GetBool());
  EXPECT_FALSE(*pref_object->user_control_disabled);

  // Ensure a block content setting results in a disabled and false pref.
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                ContentSetting::CONTENT_SETTING_BLOCK);
  pref_object = pref->GetPrefObject();
  EXPECT_FALSE(pref_object->value->GetBool());
  EXPECT_TRUE(*pref_object->user_control_disabled);

  // Confirm that the pref cannot be changed while the content setting is block.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(2).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // Ensure management state is correctly reported for all possible content
  // setting management sources.
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));
  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider),
      HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by,
            settings_api::ControlledBy::CONTROLLED_BY_EXTENSION);
  EXPECT_EQ(pref_object->enforcement,
            settings_api::Enforcement::ENFORCEMENT_ENFORCED);

  provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));
  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider), HostContentSettingsMap::SUPERVISED_PROVIDER);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by,
            settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION);
  EXPECT_EQ(pref_object->enforcement,
            settings_api::Enforcement::ENFORCEMENT_ENFORCED);

  provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(ContentSetting::CONTENT_SETTING_ALLOW));
  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider), HostContentSettingsMap::POLICY_PROVIDER);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by,
            settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY);
  EXPECT_EQ(pref_object->enforcement,
            settings_api::Enforcement::ENFORCEMENT_ENFORCED);

  // Ensure the preference cannot be changed when it is enforced.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
  EXPECT_EQ(
      map->GetDefaultContentSetting(ContentSettingsType::COOKIES, nullptr),
      ContentSetting::CONTENT_SETTING_ALLOW);
}

}  // namespace content_settings
