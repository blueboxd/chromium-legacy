// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;
}

namespace signin {
class IdentityManager;
}

class GURL;

namespace policy {
class CloudPolicyClient;
}

namespace extensions {

// An event router that observes Safe Browsing events and notifies listeners.
// The router also uploads events to the chrome reporting server side API if
// the kRealtimeReportingFeature feature is enabled.
class SafeBrowsingPrivateEventRouter : public KeyedService {
 public:
  // Feature that controls whether real-time reports are sent.
  static const base::Feature kRealtimeReportingFeature;

  // Key names used with when building the dictionary to pass to the real-time
  // reporting API.
  static const char kKeyUrl[];
  static const char kKeyUserName[];
  static const char kKeyIsPhishingUrl[];
  static const char kKeyProfileUserName[];
  static const char kKeyFileName[];
  static const char kKeyDownloadDigestSha256[];
  static const char kKeyReason[];
  static const char kKeyNetErrorCode[];
  static const char kKeyClickedThrough[];

  static const char kKeyPasswordReuseEvent[];
  static const char kKeyPasswordChangedEvent[];
  static const char kKeyDangerousDownloadEvent[];
  static const char kKeyInterstitialEvent[];

  explicit SafeBrowsingPrivateEventRouter(content::BrowserContext* context);

  ~SafeBrowsingPrivateEventRouter() override;

  // Notifies listeners that the user reused a protected password.
  void OnPolicySpecifiedPasswordReuseDetected(const GURL& url,
                                              const std::string& user_name,
                                              bool is_phishing_url);

  // Notifies listeners that the user changed the password associated with
  // |user_name|.
  void OnPolicySpecifiedPasswordChanged(const std::string& user_name);

  // Notifies listeners that the user just opened a dangerous download.
  void OnDangerousDownloadOpened(const GURL& url,
                                 const std::string& file_name,
                                 const std::string& download_digest_sha256);

  // Notifies listeners that the user saw a security interstitial.
  void OnSecurityInterstitialShown(const GURL& url,
                                   const std::string& reason,
                                   int net_error_code);

  // Notifies listeners that the user clicked-through a security interstitial.
  void OnSecurityInterstitialProceeded(const GURL& url,
                                       const std::string& reason,
                                       int net_error_code);

  void SetCloudPolicyClientForTesting(
      std::unique_ptr<policy::CloudPolicyClient> client);

 private:
  // Initialize the real-time report client if needed.  This client is used only
  // if real-time reporting is enabled, the machine is properly reigistered
  // with CBCM and the appropriate policies are enabled.
  void InitRealtimeReportingClient();

  // Report safe browsing event through real-time reporting channel, if enabled.
  void ReportRealtimeEvent(const char* name, base::Value event);

  // Returns the Gaia email address of the account signed in to the profile or
  // an empty string if the profile is not signed in.
  std::string GetProfileUserName();

  content::BrowserContext* context_;
  signin::IdentityManager* identity_manager_ = nullptr;
  EventRouter* event_router_ = nullptr;
  std::unique_ptr<policy::CloudPolicyClient> client_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
