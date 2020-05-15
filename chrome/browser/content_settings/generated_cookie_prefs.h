// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

extern const char kCookieSessionOnly[];

// The base class for generated preferences which support WebUI cookie controls
// that do not not map completely to individual preferences or content settings.
// Generated preferences allows the use of these types of controls without
// exposing supporting business logic to WebUI code.
class GeneratedCookiePrefBase
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  ~GeneratedCookiePrefBase() override;

  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

 protected:
  GeneratedCookiePrefBase(Profile* profile, const std::string& pref_name_);
  Profile* const profile_;
  HostContentSettingsMap* host_content_settings_map_;
  const std::string pref_name_;
  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};
};

class GeneratedCookieSessionOnlyPref : public GeneratedCookiePrefBase {
 public:
  explicit GeneratedCookieSessionOnlyPref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_
