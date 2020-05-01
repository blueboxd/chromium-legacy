// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_ALLOWLIST_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_ALLOWLIST_H_

#include <map>

#include "base/supports_user_data.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

class Profile;
class WebUIAllowlistProvider;

// This class is the underlying storage for WebUIAllowlistProvider, it stores a
// list of origins and permissions to be auto-granted to WebUIs. This class is
// created before HostContentSettingsMap is registered and has the same lifetime
// as the profile it's attached to. It outlives WebUIAllowlistProvider.
class WebUIAllowlist : public base::SupportsUserData::Data {
 public:
  static WebUIAllowlist* GetOrCreate(Profile* profile);

  WebUIAllowlist();
  WebUIAllowlist(const WebUIAllowlist&) = delete;
  void operator=(const WebUIAllowlist&) = delete;
  ~WebUIAllowlist() override;

  // Register auto-granted |type| permission for |origin|.
  void RegisterAutoGrantedPermission(
      const url::Origin& origin,
      ContentSettingsType type,
      ContentSetting setting = CONTENT_SETTING_ALLOW);

  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const;

  void SetWebUIAllowlistProvider(WebUIAllowlistProvider* provider);
  void ResetWebUIAllowlistProvider();

 private:
  std::map<ContentSettingsType, std::map<url::Origin, ContentSetting>>
      permissions_;
  WebUIAllowlistProvider* provider_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_ALLOWLIST_H_
