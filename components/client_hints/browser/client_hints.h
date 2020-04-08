// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class HostContentSettingsMap;

namespace client_hints {

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate,
                    public content::WebContentsUserData<ClientHints> {
 public:
  ClientHints(content::BrowserContext* context,
              network::NetworkQualityTracker* network_quality_tracker,
              HostContentSettingsMap* settings_map,
              const blink::UserAgentMetadata& user_agent_metadata);
  ~ClientHints() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      network::NetworkQualityTracker* network_quality_tracker,
      HostContentSettingsMap* settings_map,
      const blink::UserAgentMetadata& user_agent_metadata);

  // content::ClientHintsControllerDelegate:
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<network::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override;

 private:
  friend class content::WebContentsUserData<ClientHints>;

  ClientHints(content::WebContents* web_contents,
              network::NetworkQualityTracker* network_quality_tracker,
              HostContentSettingsMap* settings_map,
              const blink::UserAgentMetadata& user_agent_metadata);

  content::BrowserContext* context_ = nullptr;
  network::NetworkQualityTracker* network_quality_tracker_ = nullptr;
  HostContentSettingsMap* settings_map_ = nullptr;
  blink::UserAgentMetadata user_agent_metadata_;
  std::unique_ptr<
      content::WebContentsFrameReceiverSet<client_hints::mojom::ClientHints>>
      receiver_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClientHints);
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_H_
