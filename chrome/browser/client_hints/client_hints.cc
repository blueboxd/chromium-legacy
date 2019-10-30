// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "chrome/browser/client_hints/client_hints.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/client_hints/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"

namespace client_hints {

ClientHints::ClientHints(content::BrowserContext* context)
    : context_(context) {}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  DCHECK(g_browser_process);
  return g_browser_process->network_quality_tracker();
}

void ClientHints::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::WebEnabledClientHints* client_hints) {
  ContentSettingsForOneType client_hints_rules;
  Profile* profile = Profile::FromBrowserContext(context_);
  if (!profile)
    return;

  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_rules);
  client_hints::GetAllowedClientHintsFromSource(url, client_hints_rules,
                                                client_hints);
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(context_);
  if (!profile)
    return false;

  return HostContentSettingsMapFactory::GetForProfile(profile)
             ->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT,
                                 std::string()) == CONTENT_SETTING_ALLOW;
}

std::string ClientHints::GetAcceptLanguageString() {
  Profile* profile = Profile::FromBrowserContext(context_);
  if (!profile)
    return std::string();
  DCHECK(profile->GetPrefs());
  return profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return ::GetUserAgentMetadata();
}

}  // namespace client_hints
