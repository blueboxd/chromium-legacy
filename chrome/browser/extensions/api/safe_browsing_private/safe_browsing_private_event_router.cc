// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace extensions {

const base::Feature SafeBrowsingPrivateEventRouter::kRealtimeReportingFeature{
    "SafeBrowsingRealtimeReporting", base::FEATURE_DISABLED_BY_DEFAULT};

// Key names used with when building the dictionary to pass to the real-time
// reporting API.
const char SafeBrowsingPrivateEventRouter::kKeyUrl[] = "url";
const char SafeBrowsingPrivateEventRouter::kKeyUserName[] = "userName";
const char SafeBrowsingPrivateEventRouter::kKeyIsPhishingUrl[] =
    "isPhishingUrl";
const char SafeBrowsingPrivateEventRouter::kKeyProfileUserName[] =
    "profileUserName";
const char SafeBrowsingPrivateEventRouter::kKeyFileName[] = "fileName";
const char SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256[] =
    "downloadDigestSha256";
const char SafeBrowsingPrivateEventRouter::kKeyReason[] = "reason";
const char SafeBrowsingPrivateEventRouter::kKeyNetErrorCode[] = "netErrorCode";
const char SafeBrowsingPrivateEventRouter::kKeyClickedThrough[] =
    "clickedThrough";

const char SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent[] =
    "passwordReuseEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent[] =
    "passwordChangedEvent";
const char SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent[] =
    "dangerousDownloadEvent";
const char SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent[] =
    "interstitialEvent";

SafeBrowsingPrivateEventRouter::SafeBrowsingPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = EventRouter::Get(context_);
  InitRealtimeReportingClient();
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() {}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordReuseDetected(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url) {
  api::safe_browsing_private::PolicySpecifiedPasswordReuse params;
  params.url = url.spec();
  params.user_name = user_name;
  params.is_phishing_url = is_phishing_url;

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::
            SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_REUSE_DETECTED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (client_) {
    // Convert |params| to a real-time event dictionary and report it.
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kKeyUrl, params.url);
    event.SetStringKey(kKeyUserName, params.user_name);
    event.SetBoolKey(kKeyIsPhishingUrl, params.is_phishing_url);
    event.SetStringKey(kKeyProfileUserName, GetProfileUserName());
    ReportRealtimeEvent(kKeyPasswordReuseEvent, std::move(event));
  }
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordChanged(
    const std::string& user_name) {
  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(std::make_unique<base::Value>(user_name));
    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_CHANGED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (client_) {
    // Convert |params| to a real-time event dictionary and report it.
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kKeyUserName, user_name);
    event.SetStringKey(kKeyProfileUserName, GetProfileUserName());
    ReportRealtimeEvent(kKeyPasswordChangedEvent, std::move(event));
  }
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadOpened(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256) {
  api::safe_browsing_private::DangerousDownloadInfo params;
  params.url = url.spec();
  params.file_name = file_name;
  params.download_digest_sha256 = download_digest_sha256;
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_DANGEROUS_DOWNLOAD_OPENED,
        api::safe_browsing_private::OnDangerousDownloadOpened::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (client_) {
    // Convert |params| to a real-time event dictionary and report it.
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kKeyUrl, params.url);
    event.SetStringKey(kKeyFileName, params.file_name);
    event.SetStringKey(kKeyDownloadDigestSha256, params.download_digest_sha256);
    event.SetStringKey(kKeyProfileUserName, params.user_name);
    ReportRealtimeEvent(kKeyDangerousDownloadEvent, std::move(event));
  }
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code =
        std::make_unique<std::string>(base::NumberToString(net_error_code));
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_SHOWN,
        api::safe_browsing_private::OnSecurityInterstitialShown::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (client_) {
    // Convert |params| to a real-time event dictionary and report it.
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kKeyUrl, params.url);
    event.SetStringKey(kKeyReason, params.reason);
    event.SetIntKey(kKeyNetErrorCode, net_error_code);
    event.SetStringKey(kKeyProfileUserName, params.user_name);
    event.SetBoolKey(kKeyClickedThrough, false);
    ReportRealtimeEvent(kKeyInterstitialEvent, std::move(event));
  }
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialProceeded(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code =
        std::make_unique<std::string>(base::NumberToString(net_error_code));
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_PROCEEDED,
        api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (client_) {
    // Convert |params| to a real-time event dictionary and report it.
    base::Value event(base::Value::Type::DICTIONARY);
    event.SetStringKey(kKeyUrl, params.url);
    event.SetStringKey(kKeyReason, params.reason);
    event.SetIntKey(kKeyNetErrorCode, net_error_code);
    event.SetStringKey(kKeyProfileUserName, params.user_name);
    event.SetBoolKey(kKeyClickedThrough, true);
    ReportRealtimeEvent(kKeyInterstitialEvent, std::move(event));
  }
}

void SafeBrowsingPrivateEventRouter::SetCloudPolicyClientForTesting(
    std::unique_ptr<policy::CloudPolicyClient> client) {
  DCHECK_EQ(nullptr, client_.get());
  client_ = std::move(client);
}

void SafeBrowsingPrivateEventRouter::InitRealtimeReportingClient() {
  if (!base::FeatureList::IsEnabled(kRealtimeReportingFeature))
    return;

  // |identity_manager_| may be null in tests.  If there is no identity
  // manager don't enable the real-time reporting API since the router won't
  // be able to fill in all the info needed for the reports.
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  if (!identity_manager_)
    return;

  // |device_management_service| may be null in tests.    If there is no device
  // management service don't enable the real-time reporting API since the
  // router won't be able to create the reporting server client below.
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  if (!device_management_service)
    return;

  // Make sure we have a DM token to proceed.  During the lifetime of a running
  // chrome browser, this can only change from empty to non-empty.  There are
  // no cases where chrome starts with a dm token and then it goes away.
  // When chrome starts without a dm token and determines that one is needed,
  // browser startup is blocked until it is retrieved or an error occurs.  In
  // the latter case, chrome won't try to retrieve it again until the next
  // restart.
  //
  // Therefore, it is OK to retrieve the dm token once here on initialization
  // of the router to determine if real-time reporting can be enabled or not.
  std::string dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (dm_token.empty())
    return;

  // Make sure DeviceManagementService has been initialized.
  device_management_service->ScheduleInitialization(0);

  client_ = std::make_unique<policy::CloudPolicyClient>(
      /*machine_id=*/std::string(), /*machine_model=*/std::string(),
      /*brand_code=*/std::string(), /*ethernet_mac_address=*/std::string(),
      /*dock_mac_address=*/std::string(), /*manufacture_date=*/std::string(),
      device_management_service, g_browser_process->shared_url_loader_factory(),
      nullptr, policy::CloudPolicyClient::DeviceDMTokenCallback());

  if (!client_->is_registered()) {
    client_->SetupRegistration(
        dm_token, client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEvent(const char* name,
                                                         base::Value event) {
  // Format the current time (UTC) in RFC3339 format.
  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  std::string now_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", now_exploded.year,
      now_exploded.month, now_exploded.day_of_month, now_exploded.hour,
      now_exploded.minute, now_exploded.second, now_exploded.millisecond);

  base::Value wrapper(base::Value::Type::DICTIONARY);
  wrapper.SetStringKey("time", now_str);
  wrapper.SetKey(name, std::move(event));

  client_->UploadRealtimeReport(std::move(wrapper), base::DoNothing());
}

std::string SafeBrowsingPrivateEventRouter::GetProfileUserName() {
  // |identity_manager_| may be null is some tests.
  return identity_manager_ && identity_manager_->HasPrimaryAccount()
             ? identity_manager_->GetPrimaryAccountInfo().email
             : std::string();
}

}  // namespace extensions
