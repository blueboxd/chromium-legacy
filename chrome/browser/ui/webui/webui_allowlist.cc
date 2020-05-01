// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_allowlist.h"
#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_allowlist_provider.h"
#include "content/public/common/url_constants.h"

const char kWebUIAllowlistKeyName[] = "WebUIAllowlist";

namespace {

class AllowlistRuleIterator : public content_settings::RuleIterator {
  using MapType = std::map<url::Origin, ContentSetting>;

 public:
  explicit AllowlistRuleIterator(const MapType& map)
      : it_(map.cbegin()), end_(map.cend()) {}
  AllowlistRuleIterator(const AllowlistRuleIterator&) = delete;
  void operator=(const AllowlistRuleIterator&) = delete;
  ~AllowlistRuleIterator() override = default;

  bool HasNext() const override { return it_ != end_; }

  content_settings::Rule Next() override {
    const auto& origin = it_->first;
    const auto& setting = it_->second;
    it_++;
    return content_settings::Rule(
        ContentSettingsPattern::FromURLNoWildcard(origin.GetURL()),
        ContentSettingsPattern::Wildcard(), base::Value(setting), base::Time(),
        content_settings::SessionModel::Durable);
  }

 private:
  MapType::const_iterator it_;
  const MapType::const_iterator end_;
};

}  // namespace

// static
WebUIAllowlist* WebUIAllowlist::GetOrCreate(Profile* profile) {
  if (!profile->GetUserData(kWebUIAllowlistKeyName)) {
    profile->SetUserData(kWebUIAllowlistKeyName,
                         std::make_unique<WebUIAllowlist>());
  }
  return static_cast<WebUIAllowlist*>(
      profile->GetUserData(kWebUIAllowlistKeyName));
}

WebUIAllowlist::WebUIAllowlist() = default;

WebUIAllowlist::~WebUIAllowlist() = default;

void WebUIAllowlist::RegisterAutoGrantedPermission(const url::Origin& origin,
                                                   ContentSettingsType type,
                                                   ContentSetting setting) {
  // We only support auto-granting permissions to chrome://,
  // chrome-untrusted://, and devtools:// schemes.
  DCHECK(origin.scheme() == content::kChromeUIScheme ||
         origin.scheme() == content::kChromeUIUntrustedScheme ||
         origin.scheme() == content::kChromeDevToolsScheme);

  permissions_[type][origin] = setting;

  // Notify the provider. |provider_| can be nullptr if
  // HostContentSettingsRegistry is shutting down i.e. when Chrome shuts down.
  if (provider_) {
    auto primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin.GetURL());
    auto secondary_pattern = ContentSettingsPattern::Wildcard();
    provider_->NotifyContentSettingChange(primary_pattern, secondary_pattern,
                                          type);
  }
}

void WebUIAllowlist::SetWebUIAllowlistProvider(
    WebUIAllowlistProvider* provider) {
  provider_ = provider;
}

void WebUIAllowlist::ResetWebUIAllowlistProvider() {
  provider_ = nullptr;
}

std::unique_ptr<content_settings::RuleIterator> WebUIAllowlist::GetRuleIterator(
    ContentSettingsType content_type) const {
  const auto& type_to_origin_rules = permissions_.find(content_type);
  if (type_to_origin_rules != permissions_.cend()) {
    return std::make_unique<AllowlistRuleIterator>(
        type_to_origin_rules->second);
  }

  return nullptr;
}
