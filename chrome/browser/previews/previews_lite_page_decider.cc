// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_decider.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"

namespace {
const char kUserNeedsNotification[] =
    "previews.litepage.user-needs-notification";
const char kHostBlacklist[] = "previews.litepage.host-blacklist";

const size_t kMaxBlacklistEntries = 30;

// Cleans up the given host blacklist by removing all stale (expiry has passed)
// entries. If after removing all stale entries, the blacklist is still over
// capacity, then remove the entry with the closest expiration.
void RemoveStaleBlacklistEntries(base::DictionaryValue* dict) {
  std::vector<std::string> keys_to_delete;

  base::Time min_value = base::Time::Max();
  std::string min_key;
  for (const auto& iter : dict->DictItems()) {
    base::Time value = base::Time::FromDoubleT(iter.second.GetDouble());

    // Delete all stale entries.
    if (value <= base::Time::Now()) {
      keys_to_delete.push_back(iter.first);
      continue;
    }

    // Record the closest expiration in case we need it later on.
    if (value < min_value) {
      min_value = value;
      min_key = iter.first;
    }
  }

  // Remove all expired entries.
  for (const std::string& key : keys_to_delete)
    dict->RemoveKey(key);

  // Remove the closest expiration if needed.
  if (dict->DictSize() > kMaxBlacklistEntries)
    dict->RemoveKey(min_key);

  DCHECK_GE(kMaxBlacklistEntries, dict->DictSize());
}

void PreconnectToLitePagesServer(content::BrowserContext* browser_context) {
  predictors::LoadingPredictor* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (!loading_predictor || !loading_predictor->preconnect_manager())
    return;

  loading_predictor->preconnect_manager()->StartPreconnectUrl(
      previews::params::GetLitePagePreviewsDomainURL(), true);
}

}  // namespace

// This WebContentsObserver watches the rest of the current navigation shows a
// notification to the user that this preview now exists and will be used on
// future eligible page loads. This is only done if the navigations finishes on
// the same URL as the one when it began. After finishing the navigation, |this|
// will be removed as an observer.
class UserNotificationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<UserNotificationWebContentsObserver> {
 public:
  void SetUIShownCallback(base::OnceClosure callback) {
    ui_shown_callback_ = std::move(callback);
  }

 private:
  friend class content::WebContentsUserData<
      UserNotificationWebContentsObserver>;

  explicit UserNotificationWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DestroySelf() {
    content::WebContents* old_web_contents = web_contents();
    Observe(nullptr);
    old_web_contents->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override {
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (ui_shown_callback_ && handle->GetNetErrorCode() == net::OK) {
      PreviewsLitePageInfoBarDelegate::Create(web_contents());
      std::move(ui_shown_callback_).Run();
    }
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  base::OnceClosure ui_shown_callback_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNotificationWebContentsObserver)

PreviewsLitePageDecider::PreviewsLitePageDecider(
    content::BrowserContext* browser_context)
    : clock_(base::DefaultTickClock::GetInstance()),
      page_id_(base::RandUint64()),
      drp_settings_(nullptr),
      pref_service_(nullptr),
      host_bypass_blacklist_(std::make_unique<base::DictionaryValue>()),
      drp_headers_valid_(false) {
  if (!browser_context)
    return;

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  if (!drp_settings)
    return;

  DCHECK(!browser_context->IsOffTheRecord());

  // TODO(crbug.com/971918): Remove once a more robust prober is setup.
  if (drp_settings->IsDataReductionProxyEnabled()) {
    PreconnectToLitePagesServer(browser_context);
  }

  pref_service_ = Profile::FromBrowserContext(browser_context)->GetPrefs();
  host_bypass_blacklist_ =
      pref_service_->GetDictionary(kHostBlacklist)->CreateDeepCopy();

  // Note: This switch has no effect if |drp_settings| was null since
  // |host_bypass_blacklist_| would be empty anyways.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kClearLitePageRedirectLocalBlacklist)) {
    host_bypass_blacklist_->Clear();
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
  }

  // Add |this| as an observer to DRP, but if DRP is already initialized, check
  // the prefs now.
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
  if (drp_settings_->Config()) {
    OnSettingsInitialized();
    OnProxyRequestHeadersChanged(drp_settings->GetProxyRequestHeaders());
  }
}

PreviewsLitePageDecider::~PreviewsLitePageDecider() = default;

// static
void PreviewsLitePageDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kUserNeedsNotification, true);
  registry->RegisterDictionaryPref(kHostBlacklist);
}

// static
std::unique_ptr<content::NavigationThrottle>
PreviewsLitePageDecider::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  DCHECK(handle->GetWebContents());
  DCHECK(handle->GetWebContents()->GetBrowserContext());

  if (!handle->IsInMainFrame())
    return nullptr;

  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      base::FeatureList::IsEnabled(
          previews::features::kHTTPSServerPreviewsUsingURLLoader)) {
    return nullptr;
  }

  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!previews_service)
    return nullptr;
  DCHECK(!browser_context->IsOffTheRecord());

  PreviewsLitePageDecider* decider =
      previews_service->previews_lite_page_decider();
  DCHECK(decider);

  bool drp_enabled = decider->drp_settings_->IsDataReductionProxyEnabled();
  bool preview_enabled = previews::params::ArePreviewsAllowed() &&
                         previews::params::IsLitePageServerPreviewsEnabled();

  // Always create a navigation throttle if the feature is enabled. The throttle
  // itself will check the PreviewsState bit for triggering.
  if (drp_enabled && preview_enabled) {
    return std::make_unique<PreviewsLitePageNavigationThrottle>(handle,
                                                                decider);
  }

  return nullptr;
}

// static
uint64_t PreviewsLitePageDecider::GeneratePageIdForWebContents(
    content::WebContents* web_contents) {
  return PreviewsLitePageDecider::GeneratePageIdForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

// static
uint64_t PreviewsLitePageDecider::GeneratePageIdForProfile(Profile* profile) {
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile);
  return previews_service
             ? previews_service->previews_lite_page_decider()->GeneratePageID()
             : 0;
}

void PreviewsLitePageDecider::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string drp_header;
  drp_headers_valid_ =
      headers.GetHeader(data_reduction_proxy::chrome_proxy_header(),
                        &drp_header) &&
      (drp_header.find(",s=") != std::string::npos ||
       drp_header.find(" s=") != std::string::npos ||
       base::StartsWith(drp_header, "s=", base::CompareCase::SENSITIVE));

  // This is done so that successive page ids cannot be used to track users
  // across sessions. These sessions are contained in the chrome-proxy header.
  page_id_ = base::RandUint64();
}

void PreviewsLitePageDecider::OnSettingsInitialized() {
  // The notification only needs to be shown if the user has never seen it
  // before, and is an existing Data Saver user.
  if (!pref_service_->GetBoolean(kUserNeedsNotification)) {
    need_to_show_notification_ = false;
  } else if (drp_settings_->IsDataReductionProxyEnabled()) {
    need_to_show_notification_ = true;
  } else {
    need_to_show_notification_ = false;
    pref_service_->SetBoolean(kUserNeedsNotification, false);
  }
}

void PreviewsLitePageDecider::Shutdown() {
  if (drp_settings_)
    drp_settings_->RemoveDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageDecider::SetClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void PreviewsLitePageDecider::SetDRPSettingsForTesting(
    data_reduction_proxy::DataReductionProxySettings* drp_settings) {
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageDecider::ClearBlacklist() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_bypass_blacklist_->Clear();
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
}

void PreviewsLitePageDecider::ClearStateForTesting() {
  single_bypass_.clear();
  host_bypass_blacklist_->Clear();
}

void PreviewsLitePageDecider::SetUserHasSeenUINotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  need_to_show_notification_ = false;
  pref_service_->SetBoolean(kUserNeedsNotification, false);
}

void PreviewsLitePageDecider::SetServerUnavailableFor(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks retry_at = clock_->NowTicks() + retry_after;
  if (!retry_at_.has_value() || retry_at > retry_at_)
    retry_at_ = retry_at;
}

bool PreviewsLitePageDecider::IsServerUnavailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!retry_at_.has_value())
    return false;
  bool server_loadshedding = retry_at_ > clock_->NowTicks();
  if (!server_loadshedding)
    retry_at_.reset();
  return server_loadshedding;
}

void PreviewsLitePageDecider::AddSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Garbage collect any old entries while looking for the one for |url|.
  auto entry = single_bypass_.end();
  for (auto iter = single_bypass_.begin(); iter != single_bypass_.end();
       /* no increment */) {
    if (iter->second < clock_->NowTicks()) {
      iter = single_bypass_.erase(iter);
      continue;
    }
    if (iter->first == url)
      entry = iter;
    ++iter;
  }

  // Update the entry for |url|.
  const base::TimeTicks ttl =
      clock_->NowTicks() + base::TimeDelta::FromMinutes(5);
  if (entry == single_bypass_.end()) {
    single_bypass_.emplace(url, ttl);
    return;
  }
  entry->second = ttl;
}

bool PreviewsLitePageDecider::CheckSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = single_bypass_.find(url);
  if (entry == single_bypass_.end())
    return false;
  return entry->second >= clock_->NowTicks();
}

uint64_t PreviewsLitePageDecider::GeneratePageID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
}

void PreviewsLitePageDecider::ReportDataSavings(int64_t network_bytes,
                                                int64_t original_bytes,
                                                const std::string& host) {
  if (!drp_settings_ || !drp_settings_->data_reduction_proxy_service())
    return;

  // The total data usage is tracked for all data in Chrome, so we only need to
  // update the savings.
  int64_t data_saved = original_bytes - network_bytes;
  drp_settings_->data_reduction_proxy_service()->UpdateDataUseForHost(
      0, data_saved, host);

  drp_settings_->data_reduction_proxy_service()->UpdateContentLengths(
      0, data_saved, true /* data_reduction_proxy_enabled */,
      data_reduction_proxy::DataReductionProxyRequestType::
          VIA_DATA_REDUCTION_PROXY,
      "text/html", true /* is_user_traffic */,
      data_use_measurement::DataUseUserData::DataUseContentType::
          MAIN_FRAME_HTML,
      0);
}

bool PreviewsLitePageDecider::NeedsToNotifyUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar)) {
    return false;
  }
  return need_to_show_notification_;
}

void PreviewsLitePageDecider::NotifyUser(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(need_to_show_notification_);
  DCHECK(!UserNotificationWebContentsObserver::FromWebContents(web_contents));

  UserNotificationWebContentsObserver::CreateForWebContents(web_contents);
  UserNotificationWebContentsObserver* observer =
      UserNotificationWebContentsObserver::FromWebContents(web_contents);

  // base::Unretained is safe here because |this| outlives |web_contents|.
  observer->SetUIShownCallback(
      base::BindOnce(&PreviewsLitePageDecider::SetUserHasSeenUINotification,
                     base::Unretained(this)));
}

void PreviewsLitePageDecider::BlacklistBypassedHost(const std::string& host,
                                                    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is an existing entry, intentionally update it.
  host_bypass_blacklist_->SetKey(
      host, base::Value((base::Time::Now() + duration).ToDoubleT()));

  RemoveStaleBlacklistEntries(host_bypass_blacklist_.get());
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
}

bool PreviewsLitePageDecider::HostBlacklistedFromBypass(
    const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value* value = host_bypass_blacklist_->FindKey(host);
  if (!value)
    return false;

  DCHECK(value->is_double());
  base::Time expiry = base::Time::FromDoubleT(value->GetDouble());
  return expiry > base::Time::Now();
}
