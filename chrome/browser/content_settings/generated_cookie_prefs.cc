// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_cookie_prefs.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace settings_api = extensions::api::settings_private;

namespace content_settings {

namespace {

// Implements the small subset of possible cookie controls management state
// needed to support testing of the SettingsPrivate API. Only device policy
// applied to the content setting is considered. Other sources of management
// and the management of third party cookie blocking preferences are ignored.
// This code is currently not reachable by production code.
// TODO(crbug.com/1063265): Move complete management logic and exhaustive
//    test cases from site_settings_helper.cc into this file and associated
//    unit tests and remove this function.
void ApplyCookieControlsManagedState(settings_api::PrefObject* pref_object,
                                     Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::string content_setting_provider;
  auto content_setting = map->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);

  if (content_setting_source == SettingSource::SETTING_SOURCE_POLICY) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  } else {
    // Other sources of management are currently ignored (see function comment).
    return;
  }

  // If the content setting is not set to block, the user is still able to
  // select from available third party cookie blocking options.
  if (content_setting != ContentSetting::CONTENT_SETTING_BLOCK) {
    pref_object->user_selectable_values =
        std::make_unique<std::vector<std::unique_ptr<base::Value>>>();
    pref_object->user_selectable_values->push_back(
        std::make_unique<base::Value>(
            static_cast<int>(CookiePrimarySetting::ALLOW_ALL)));
    pref_object->user_selectable_values->push_back(
        std::make_unique<base::Value>(static_cast<int>(
            CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO)));
    pref_object->user_selectable_values->push_back(
        std::make_unique<base::Value>(
            static_cast<int>(CookiePrimarySetting::BLOCK_THIRD_PARTY)));
  }
}

bool IsDefaultCookieContentSettingUserControlled(HostContentSettingsMap* map) {
  std::string content_setting_provider;
  map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  return content_setting_source == SettingSource::SETTING_SOURCE_USER;
}

// Updates all user modifiable cookie content settings and preferences to match
// the provided |controls_mode| and |content_setting|. This provides a
// consistent interface to updating these when they are partially managed.
// Returns SetPrefResult::SUCCESS if any settings could be changed, and
// SetPrefResult::PREF_NOT_MODIFIABLE if no setting could be changed.
extensions::settings_private::SetPrefResult SetAllCookieSettings(
    Profile* profile,
    CookieControlsMode controls_mode,
    ContentSetting content_setting) {
  bool setting_changed = false;

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (IsDefaultCookieContentSettingUserControlled(map)) {
    map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                  content_setting);
    setting_changed = true;
  }

  auto* pref_service = profile->GetPrefs();
  if (pref_service->FindPreference(prefs::kBlockThirdPartyCookies)
          ->IsUserControlled()) {
    pref_service->SetBoolean(
        prefs::kBlockThirdPartyCookies,
        controls_mode == CookieControlsMode::kBlockThirdParty);
    setting_changed = true;
  }
  if (pref_service->FindPreference(prefs::kCookieControlsMode)
          ->IsUserControlled()) {
    pref_service->SetInteger(prefs::kCookieControlsMode,
                             static_cast<int>(controls_mode));
    setting_changed = true;
  }

  return setting_changed
             ? extensions::settings_private::SetPrefResult::SUCCESS
             : extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
}

}  // namespace

const char kCookiePrimarySetting[] = "generated.cookie_primary_setting";
const char kCookieSessionOnly[] = "generated.cookie_session_only";

GeneratedCookiePrefBase::GeneratedCookiePrefBase(Profile* profile,
                                                 const std::string& pref_name)
    : profile_(profile), pref_name_(pref_name) {
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_settings_observer_.Add(host_content_settings_map_);

  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&GeneratedCookiePrefBase::OnCookiePreferencesChanged,
                 base::Unretained(this)));
  user_prefs_registrar_.Add(
      prefs::kCookieControlsMode,
      base::Bind(&GeneratedCookiePrefBase::OnCookiePreferencesChanged,
                 base::Unretained(this)));
}

GeneratedCookiePrefBase::~GeneratedCookiePrefBase() = default;

void GeneratedCookiePrefBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type == ContentSettingsType::COOKIES) {
    NotifyObservers(pref_name_);
  }
}

void GeneratedCookiePrefBase::OnCookiePreferencesChanged() {
  NotifyObservers(pref_name_);
}

GeneratedCookiePrimarySettingPref::GeneratedCookiePrimarySettingPref(
    Profile* profile)
    : GeneratedCookiePrefBase(profile, kCookiePrimarySetting) {}

extensions::settings_private::SetPrefResult
GeneratedCookiePrimarySettingPref::SetPref(const base::Value* value) {
  if (!value->is_int())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  auto current_content_setting =
      host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::COOKIES, nullptr);

  auto allow_setting =
      current_content_setting != ContentSetting::CONTENT_SETTING_BLOCK
          ? current_content_setting
          : ContentSetting::CONTENT_SETTING_ALLOW;

  auto selection = static_cast<CookiePrimarySetting>(value->GetInt());
  switch (selection) {
    case (CookiePrimarySetting::ALLOW_ALL):
      return SetAllCookieSettings(profile_, CookieControlsMode::kOff,
                                  allow_setting);
    case (CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO):
      return SetAllCookieSettings(profile_, CookieControlsMode::kIncognitoOnly,
                                  allow_setting);
    case (CookiePrimarySetting::BLOCK_THIRD_PARTY):
      return SetAllCookieSettings(
          profile_, CookieControlsMode::kBlockThirdParty, allow_setting);
    case (CookiePrimarySetting::BLOCK_ALL):
      return SetAllCookieSettings(profile_,
                                  CookieControlsMode::kBlockThirdParty,
                                  ContentSetting::CONTENT_SETTING_BLOCK);
    default:
      return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }
}

std::unique_ptr<extensions::api::settings_private::PrefObject>
GeneratedCookiePrimarySettingPref::GetPrefObject() const {
  auto pref_object =
      std::make_unique<extensions::api::settings_private::PrefObject>();
  pref_object->key = pref_name_;
  pref_object->type = extensions::api::settings_private::PREF_TYPE_NUMBER;

  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, nullptr);

  auto block_third_party_pref_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies);
  auto cookie_controls_pref_value = static_cast<CookieControlsMode>(
      profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  if (content_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(CookiePrimarySetting::BLOCK_ALL));
  } else if (block_third_party_pref_enabled) {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(CookiePrimarySetting::BLOCK_THIRD_PARTY));
  } else if (cookie_controls_pref_value == CookieControlsMode::kIncognitoOnly &&
             base::FeatureList::IsEnabled(kImprovedCookieControls)) {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO));
  } else {
    pref_object->value = std::make_unique<base::Value>(
        static_cast<int>(CookiePrimarySetting::ALLOW_ALL));
  }

  ApplyCookieControlsManagedState(pref_object.get(), profile_);

  return pref_object;
}

GeneratedCookieSessionOnlyPref::GeneratedCookieSessionOnlyPref(Profile* profile)
    : GeneratedCookiePrefBase(profile, kCookieSessionOnly) {}

extensions::settings_private::SetPrefResult
GeneratedCookieSessionOnlyPref::SetPref(const base::Value* value) {
  if (!value->is_bool())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  if (!IsDefaultCookieContentSettingUserControlled(host_content_settings_map_))
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  if (host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::COOKIES, nullptr) ==
      ContentSetting::CONTENT_SETTING_BLOCK)
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES,
      value->GetBool() ? ContentSetting::CONTENT_SETTING_SESSION_ONLY
                       : ContentSetting::CONTENT_SETTING_ALLOW);

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

std::unique_ptr<settings_api::PrefObject>
GeneratedCookieSessionOnlyPref::GetPrefObject() const {
  auto pref_object = std::make_unique<settings_api::PrefObject>();
  pref_object->key = pref_name_;
  pref_object->type = settings_api::PREF_TYPE_BOOLEAN;

  std::string content_setting_provider;
  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);

  pref_object->user_control_disabled = std::make_unique<bool>(
      content_setting == ContentSetting::CONTENT_SETTING_BLOCK);
  pref_object->value = std::make_unique<base::Value>(
      content_setting == ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  // Content settings can be managed via policy, extension or supervision, but
  // cannot be recommended.
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  if (content_setting_source == SettingSource::SETTING_SOURCE_POLICY) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_EXTENSION) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_SUPERVISED) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }

  return pref_object;
}

}  // namespace content_settings
