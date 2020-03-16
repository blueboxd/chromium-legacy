// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

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
class DeviceManagementService;
}

namespace safe_browsing {
class BinaryUploadService;
class DlpDeepScanningVerdict;
}

#if defined(OS_CHROMEOS)

namespace user_manager {
class User;
}

#endif

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
  static const char kKeyTriggeredRuleId[];
  static const char kKeyTriggeredRuleName[];
  static const char kKeyTriggeredRuleResourceName[];
  static const char kKeyTriggeredRuleSeverity[];
  static const char kKeyTriggeredRuleAction[];
  static const char kKeyMatchedDetectors[];
  static const char kKeyMatchedDetectorId[];
  static const char kKeyMatchedDetectorName[];
  static const char kKeyMatchedDetectorType[];
  static const char kKeyTriggeredRuleInfo[];
  static const char kKeyThreatType[];
  static const char kKeyContentType[];
  static const char kKeyContentSize[];
  static const char kKeyTrigger[];

  static const char kKeyPasswordReuseEvent[];
  static const char kKeyPasswordChangedEvent[];
  static const char kKeyDangerousDownloadEvent[];
  static const char kKeyInterstitialEvent[];
  static const char kKeySensitiveDataEvent[];
  static const char kKeyUnscannedFileEvent[];

  // String constants for the "trigger" event field.
  static const char kTriggerFileDownload[];
  static const char kTriggerFileUpload[];
  static const char kTriggerWebContentUpload[];

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
                                 const std::string& download_digest_sha256,
                                 const std::string& mime_type,
                                 const int64_t content_size);

  // Notifies listeners that the user saw a security interstitial.
  void OnSecurityInterstitialShown(const GURL& url,
                                   const std::string& reason,
                                   int net_error_code);

  // Notifies listeners that the user clicked-through a security interstitial.
  void OnSecurityInterstitialProceeded(const GURL& url,
                                       const std::string& reason,
                                       int net_error_code);

  // Notifies listeners that deep scanning detected a dangerous download.
  void OnDangerousDeepScanningResult(const GURL& url,
                                     const std::string& file_name,
                                     const std::string& download_digest_sha256,
                                     const std::string& threat_type,
                                     const std::string& mime_type,
                                     const std::string& trigger,
                                     const int64_t content_size);

  // Notifies listeners that scanning for sensitive data detected a violation.
  void OnSensitiveDataEvent(
      const safe_browsing::DlpDeepScanningVerdict& verdict,
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      const int64_t content_size);

  // Notifies listeners that scanning for sensitive data detected a violation.
  void OnSensitiveDataWarningBypassed(const GURL& url,
                                      const std::string& file_name,
                                      const std::string& download_digest_sha256,
                                      const std::string& mime_type,
                                      const std::string& trigger,
                                      const int64_t content_size);

  // Notifies listeners that deep scanning failed, for the given |reason|.
  void OnUnscannedFileEvent(const GURL& url,
                            const std::string& file_name,
                            const std::string& download_digest_sha256,
                            const std::string& mime_type,
                            const std::string& trigger,
                            const std::string& reason,
                            const int64_t content_size);

  // Notifies listeners that the user saw a download warning.
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadWarning(const GURL& url,
                                  const std::string& file_name,
                                  const std::string& download_digest_sha256,
                                  const std::string& threat_type,
                                  const std::string& mime_type,
                                  const int64_t content_size);

  // Notifies listeners that the user bypassed a download warning.
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadWarningBypassed(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& threat_type,
      const std::string& mime_type,
      const int64_t content_size);

  // Returns true if enterprise real-time reporting should be initialized,
  // checking both the feature flag and whether the browser is managed.  This
  // function is public so that it can called in tests.
  static bool ShouldInitRealtimeReportingClient();

  void SetCloudPolicyClientForTesting(policy::CloudPolicyClient* client);

  void SetBinaryUploadServiceForTesting(
      safe_browsing::BinaryUploadService* binary_upload_service);

 protected:
  // Callback to report safe browsing event through real-time reporting channel,
  // if the browser is authorized to do so. Declared as protected to be called
  // directly by tests. Events are created lazily to avoid doing useless work if
  // they are discarded.
  using EventBuilder = base::OnceCallback<base::Value()>;
  void ReportRealtimeEventCallback(const std::string& name,
                                   EventBuilder event_builder,
                                   bool authorized);

 private:
  // Initialize the real-time report client if needed.  This client is used only
  // if real-time reporting is enabled, the machine is properly reigistered
  // with CBCM and the appropriate policies are enabled.
  void InitRealtimeReportingClient();

  // Initialize DeviceManagementService and |client_| after validating the
  // browser can upload data.
  void InitRealtimeReportingClientCallback(
      policy::DeviceManagementService* device_management_service,
      bool authorized);

  // Continues execution if the client is authorized to do so.
  void IfAuthorized(base::OnceCallback<void(bool)> cont);

  // Determines if the real-time reporting feature is enabled.
  bool IsRealtimeReportingEnabled();

  // Called whenever the real-time reporting policy changes.
  void RealtimeReportingPrefChanged(const std::string& pref);

  // Report safe browsing event through real-time reporting channel, if enabled.
  // Declared as virtual for tests.
  virtual void ReportRealtimeEvent(const std::string&,
                                   EventBuilder event_builder);

  // Create a privately owned cloud policy client for events routing.
  void CreatePrivateCloudPolicyClient(
      const std::string& policy_client_desc,
      policy::DeviceManagementService* device_management_service,
      const std::string& client_id,
      const std::string& dm_token);

  // Handle the availability of a cloud policy client.
  void OnCloudPolicyClientAvailable(const std::string& policy_client_desc,
                                    policy::CloudPolicyClient* client);

#if defined(OS_CHROMEOS)

  // Return the Chrome OS user who is subject to reporting, or nullptr if
  // the user cannot be deterined.
  static const user_manager::User* GetChromeOSUser();

#endif

  // Determines if real-time reporting is available based on platform and user.
  static bool IsRealtimeReportingAvailable();

  // Returns the Gaia email address of the account signed in to the profile or
  // an empty string if the profile is not signed in.
  std::string GetProfileUserName() const;

  content::BrowserContext* context_;
  signin::IdentityManager* identity_manager_ = nullptr;
  EventRouter* event_router_ = nullptr;
  safe_browsing::BinaryUploadService* binary_upload_service_ = nullptr;
  // The cloud policy client used to upload events to the cloud. This client
  // is never used to fetch policies. This pointer is not owned by the class.
  policy::CloudPolicyClient* client_ = nullptr;
  // The |private_client_| is used on platforms where we cannot just get a
  // client and we create our own (used through |client_|).
  std::unique_ptr<policy::CloudPolicyClient> private_client_;
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<SafeBrowsingPrivateEventRouter> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
