// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_cookie_prefs.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace {

bool IsDefaultCookieContentSettingUserControlled(HostContentSettingsMap* map) {
  std::string content_setting_provider;
  map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  return content_setting_source ==
         content_settings::SettingSource::SETTING_SOURCE_USER;
}

}  // namespace

namespace settings_api = extensions::api::settings_private;

namespace content_settings {

const char kCookieSessionOnly[] = "generated.cookie_session_only";

GeneratedCookiePrefBase::GeneratedCookiePrefBase(Profile* profile,
                                                 const std::string& pref_name)
    : profile_(profile), pref_name_(pref_name) {
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_settings_observer_.Add(host_content_settings_map_);
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
