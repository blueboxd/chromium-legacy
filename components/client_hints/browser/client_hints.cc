// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <functional>
#include <string>

#include "components/client_hints/browser/client_hints.h"

#include "base/metrics/histogram_macros.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/origin_util.h"

namespace client_hints {

ClientHints::ClientHints(
    content::BrowserContext* context,
    network::NetworkQualityTracker* network_quality_tracker,
    HostContentSettingsMap* settings_map,
    const blink::UserAgentMetadata& user_agent_metadata)
    : context_(context),
      network_quality_tracker_(network_quality_tracker),
      settings_map_(settings_map),
      user_agent_metadata_(user_agent_metadata) {
  DCHECK(context_);
  DCHECK(network_quality_tracker_);
  DCHECK(settings_map_);
}

// static
void ClientHints::CreateForWebContents(
    content::WebContents* web_contents,
    network::NetworkQualityTracker* network_quality_tracker,
    HostContentSettingsMap* settings_map,
    const blink::UserAgentMetadata& user_agent_metadata) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new ClientHints(web_contents, network_quality_tracker,
                                         settings_map, user_agent_metadata)));
  }
}

ClientHints::~ClientHints() = default;

network::NetworkQualityTracker* ClientHints::GetNetworkQualityTracker() {
  return network_quality_tracker_;
}

void ClientHints::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::WebEnabledClientHints* client_hints) {
  ContentSettingsForOneType client_hints_rules;
  settings_map_->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                                       std::string(), &client_hints_rules);
  client_hints::GetAllowedClientHintsFromSource(url, client_hints_rules,
                                                client_hints);
}

bool ClientHints::IsJavaScriptAllowed(const GURL& url) {
  return settings_map_->GetContentSetting(
             url, url, ContentSettingsType::JAVASCRIPT, std::string()) ==
         CONTENT_SETTING_ALLOW;
}

blink::UserAgentMetadata ClientHints::GetUserAgentMetadata() {
  return user_agent_metadata_;
}

ClientHints::ClientHints(
    content::WebContents* web_contents,
    network::NetworkQualityTracker* network_quality_tracker,
    HostContentSettingsMap* settings_map,
    const blink::UserAgentMetadata& user_agent_metadata)
    : ClientHints(web_contents->GetBrowserContext(),
                  network_quality_tracker,
                  settings_map,
                  user_agent_metadata) {
  receiver_ = std::make_unique<
      content::WebContentsFrameReceiverSet<client_hints::mojom::ClientHints>>(
      web_contents, this);
}

void ClientHints::PersistClientHints(
    const url::Origin& primary_origin,
    const std::vector<network::mojom::WebClientHintsType>& client_hints,
    base::TimeDelta expiration_duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const GURL primary_url = primary_origin.GetURL();

  // TODO(tbansal): crbug.com/735518. Consider killing the renderer that sent
  // the malformed IPC.
  if (!primary_url.is_valid() || !content::IsOriginSecure(primary_url))
    return;

  DCHECK_LE(
      client_hints.size(),
      static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) + 1);

  if (client_hints.size() >
      (static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) +
       1)) {
    // Return early if the list does not have the right number of values.
    // Persisting wrong number of values to the disk may cause errors when
    // reading them back in the future.
    return;
  }

  if (expiration_duration <= base::TimeDelta::FromSeconds(0))
    return;

  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->Reserve(client_hints.size());

  // Use wall clock since the expiration time would be persisted across embedder
  // restarts.
  double expiration_time =
      (base::Time::Now() + expiration_duration).ToDoubleT();

  for (const auto& entry : client_hints)
    expiration_times_list->AppendInteger(static_cast<int>(entry));

  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);

  // TODO(tbansal): crbug.com/735518. Disable updates to client hints settings
  // when cookies are disabled for |primary_origin|.
  settings_map_->SetWebsiteSettingDefaultScope(
      primary_url, GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::move(expiration_times_dictionary));

  UMA_HISTOGRAM_EXACT_LINEAR("ClientHints.UpdateEventCount", 1, 2);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientHints)

}  // namespace client_hints
