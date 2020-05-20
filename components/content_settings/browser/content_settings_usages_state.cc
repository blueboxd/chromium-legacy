// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_usages_state.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/url_formatter/url_formatter.h"

ContentSettingsUsagesState::ContentSettingsUsagesState(
    content_settings::TabSpecificContentSettings::Delegate* delegate_,
    ContentSettingsType type)
    : delegate_(delegate_), type_(type) {}

ContentSettingsUsagesState::~ContentSettingsUsagesState() {}

void ContentSettingsUsagesState::OnPermissionSet(const GURL& requesting_origin,
                                                 bool allowed) {
  state_map_[requesting_origin] =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
}

void ContentSettingsUsagesState::DidNavigate(const GURL& url,
                                             const GURL& previous_url) {
  embedder_url_ = url;
  if (state_map_.empty())
    return;
  if (previous_url.GetOrigin() != url.GetOrigin()) {
    state_map_.clear();
    return;
  }
  // We're in the same origin, check if there's any icon to be displayed.
  unsigned int tab_state_flags = 0;
  GetDetailedInfo(nullptr, &tab_state_flags);
  if (!(tab_state_flags & TABSTATE_HAS_ANY_ICON))
    state_map_.clear();
}

void ContentSettingsUsagesState::ClearStateMap() {
  state_map_.clear();
}

void ContentSettingsUsagesState::GetDetailedInfo(
    FormattedHostsPerState* formatted_hosts_per_state,
    unsigned int* tab_state_flags) const {
  DCHECK(tab_state_flags);
  DCHECK(embedder_url_.is_valid());
  // This logic is used only for |ContentSettingsType::GEOLOCATION| and
  // |ContentSettingsType::MIDI_SYSEX|.
  DCHECK(type_ == ContentSettingsType::GEOLOCATION ||
         type_ == ContentSettingsType::MIDI_SYSEX);
  ContentSetting default_setting =
      delegate_->GetSettingsMap()->GetDefaultContentSetting(type_, nullptr);
  std::set<std::string> formatted_hosts;
  std::set<std::string> repeated_formatted_hosts;

  // Build a set of repeated formatted hosts.
  for (const auto& i : state_map_) {
    std::string formatted_host = GURLToFormattedHost(i.first);
    if (!formatted_hosts.insert(formatted_host).second) {
      repeated_formatted_hosts.insert(formatted_host);
    }
  }

  for (const auto& i : state_map_) {
    const GURL& origin = i.first;
    // The setting that was applied when the corresponding capability was last
    // requested.
    const ContentSetting& effective_setting = i.second;

    if (effective_setting == CONTENT_SETTING_ALLOW)
      *tab_state_flags |= TABSTATE_HAS_ANY_ALLOWED;
    if (formatted_hosts_per_state) {
      std::string formatted_host = GURLToFormattedHost(origin);
      std::string final_formatted_host =
          repeated_formatted_hosts.find(formatted_host) ==
                  repeated_formatted_hosts.end()
              ? formatted_host
              : origin.spec();
      (*formatted_hosts_per_state)[effective_setting].insert(
          final_formatted_host);
    }

    ContentSetting saved_setting =
        delegate_->GetSettingsMap()->GetContentSetting(origin, embedder_url_,
                                                       type_, std::string());
    ContentSetting embargo_setting =
        delegate_->GetEmbargoSetting(origin, type_);

    // |embargo_setting| can be only CONTENT_SETTING_ASK or
    // CONTENT_SETTING_BLOCK. If |saved_setting| is CONTENT_SETTING_ASK then
    // the |embargo_setting| takes effect.
    if (saved_setting == CONTENT_SETTING_ASK)
      saved_setting = embargo_setting;

    // |effective_setting| can be only CONTENT_SETTING_ALLOW or
    // CONTENT_SETTING_BLOCK.
    if (saved_setting != effective_setting)
      *tab_state_flags |= TABSTATE_HAS_CHANGED;

    if (saved_setting != default_setting)
      *tab_state_flags |= TABSTATE_HAS_EXCEPTION;

    if (saved_setting != CONTENT_SETTING_ASK)
      *tab_state_flags |= TABSTATE_HAS_ANY_ICON;
  }
}

std::string ContentSettingsUsagesState::GURLToFormattedHost(
    const GURL& url) const {
  base::string16 display_host;
  url_formatter::AppendFormattedHost(url, &display_host);
  return base::UTF16ToUTF8(display_host);
}
