// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"

namespace content_settings {

CookieSettings::CookieSettings(
    HostContentSettingsMap* host_content_settings_map,
    PrefService* prefs,
    const char* extension_scheme)
    : host_content_settings_map_(host_content_settings_map),
      extension_scheme_(extension_scheme),
      block_third_party_cookies_(false) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieSettings::OnCookiePreferencesChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kCookieControlsEnabled,
      base::Bind(&CookieSettings::OnCookiePreferencesChanged,
                 base::Unretained(this)));
  OnCookiePreferencesChanged();
}

ContentSetting CookieSettings::GetDefaultCookieSetting(
    std::string* provider_id) const {
  return host_content_settings_map_->GetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, provider_id);
}

void CookieSettings::GetCookieSettings(
    ContentSettingsForOneType* settings) const {
  host_content_settings_map_->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), settings);
}

void CookieSettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kBlockThirdPartyCookies, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kCookieControlsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void CookieSettings::SetDefaultCookieSetting(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, setting);
}

void CookieSettings::SetCookieSetting(const GURL& primary_url,
                                      ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingDefaultScope(
      primary_url, GURL(), CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      setting);
}

void CookieSettings::ResetCookieSetting(const GURL& primary_url) {
  host_content_settings_map_->SetNarrowestContentSetting(
      primary_url, GURL(), CONTENT_SETTINGS_TYPE_COOKIES,
      CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsThirdPartyAccessAllowed(const GURL& first_party_url) {
  // Use GURL() as an opaque primary url to check if any site
  // could access cookies in a 3p context on |first_party_url|.
  return IsCookieAccessAllowed(GURL(), first_party_url);
}

void CookieSettings::SetThirdPartyCookieSetting(const GURL& first_party_url,
                                                ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      setting);
}

void CookieSettings::ResetThirdPartyCookieSetting(const GURL& first_party_url) {
  host_content_settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLNoWildcard(first_party_url),
      ContentSettingsType::CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
}

bool CookieSettings::IsStorageDurable(const GURL& origin) const {
  // TODO(dgrogan): Don't use host_content_settings_map_ directly.
  // https://crbug.com/539538
  ContentSetting setting = host_content_settings_map_->GetContentSetting(
      origin /*primary*/, origin /*secondary*/,
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
      std::string() /*resource_identifier*/);
  return setting == CONTENT_SETTING_ALLOW;
}

void CookieSettings::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_change_registrar_.RemoveAll();
}

void CookieSettings::GetCookieSetting(const GURL& url,
                                      const GURL& first_party_url,
                                      content_settings::SettingSource* source,
                                      ContentSetting* cookie_setting) const {
  DCHECK(cookie_setting);
  // Auto-allow in extensions or for WebUI embedded in a secure origin.
  if (first_party_url.SchemeIs(kChromeUIScheme) &&
      url.SchemeIsCryptographic()) {
    *cookie_setting = CONTENT_SETTING_ALLOW;
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extension_scheme_) &&
      first_party_url.SchemeIs(extension_scheme_)) {
    *cookie_setting = CONTENT_SETTING_ALLOW;
    return;
  }
#endif

  // First get any host-specific settings.
  SettingInfo info;
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          url, first_party_url, CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
          &info);
  if (source)
    *source = info.source;

  // If no explicit exception has been made and third-party cookies are blocked
  // by default, apply CONTENT_SETTING_BLOCKED.
  bool block_third = info.primary_pattern.MatchesAllHosts() &&
                     info.secondary_pattern.MatchesAllHosts() &&
                     ShouldBlockThirdPartyCookies() &&
                     !first_party_url.SchemeIs(extension_scheme_);
  net::StaticCookiePolicy policy(
      net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);

  // We should always have a value, at least from the default provider.
  DCHECK(value);
  ContentSetting setting = ValueToContentSetting(value.get());
  bool block =
      block_third && policy.CanAccessCookies(url, first_party_url) != net::OK;
  *cookie_setting = block ? CONTENT_SETTING_BLOCK : setting;
}

CookieSettings::~CookieSettings() {
}

void CookieSettings::OnCookiePreferencesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool new_block_third_party_cookies =
      pref_change_registrar_.prefs()->GetBoolean(
          prefs::kBlockThirdPartyCookies) ||
      (base::FeatureList::IsEnabled(kImprovedCookieControls) &&
       pref_change_registrar_.prefs()->GetBoolean(
           prefs::kCookieControlsEnabled));

  if (block_third_party_cookies_ != new_block_third_party_cookies) {
    {
      base::AutoLock auto_lock(lock_);
      block_third_party_cookies_ = new_block_third_party_cookies;
    }
    for (Observer& obs : observers_)
      obs.OnThirdPartyCookieBlockingChanged(new_block_third_party_cookies);
  }
}

bool CookieSettings::ShouldBlockThirdPartyCookies() const {
  base::AutoLock auto_lock(lock_);
  return block_third_party_cookies_;
}

}  // namespace content_settings
