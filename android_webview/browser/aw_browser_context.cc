// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"

#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_download_manager_delegate.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/aw_quota_manager_bridge.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/aw_web_ui_controller_factory.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_whitelist_manager.h"
#include "base/base_paths_posix.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/tracked/segregated_pref_store.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {

namespace prefs {

// String that specifies the Android account type to use for Negotiate
// authentication.
const char kAuthAndroidNegotiateAccountType[] =
    "auth.android_negotiate_account_type";

// Whitelist containing servers for which Integrated Authentication is enabled.
const char kAuthServerWhitelist[] = "auth.server_whitelist";

}  // namespace prefs

namespace {

const void* const kDownloadManagerDelegateKey = &kDownloadManagerDelegateKey;

AwBrowserContext* g_browser_context = NULL;

std::unique_ptr<net::ProxyConfigServiceAndroid> CreateProxyConfigService() {
  std::unique_ptr<net::ProxyConfigServiceAndroid> config_service_android =
      std::make_unique<net::ProxyConfigServiceAndroid>(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
          base::ThreadTaskRunnerHandle::Get());

  // TODO(csharrison) Architect the wrapper better so we don't need a cast for
  // android ProxyConfigServices.
  config_service_android->set_exclude_pac_url(true);
  return config_service_android;
}

std::unique_ptr<AwSafeBrowsingWhitelistManager>
CreateSafeBrowsingWhitelistManager() {
  // Should not be called until the end of PreMainMessageLoopRun,
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  return std::make_unique<AwSafeBrowsingWhitelistManager>(
      background_task_runner, io_task_runner);
}

// Empty method to skip origin security check as DownloadManager will set its
// own method.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

}  // namespace

AwBrowserContext::AwBrowserContext(
    const base::FilePath path,
    std::unique_ptr<PrefService> pref_service,
    std::unique_ptr<policy::BrowserPolicyConnectorBase> policy_connector)
    : context_storage_path_(path),
      user_pref_service_(std::move(pref_service)),
      browser_policy_connector_(std::move(policy_connector)),
      simple_factory_key_(GetPath(), IsOffTheRecord()) {
  DCHECK(!g_browser_context);
  g_browser_context = this;
  SimpleKeyMap::GetInstance()->Associate(this, &simple_factory_key_);

  BrowserContext::Initialize(this, path);

  pref_change_registrar_.Init(user_pref_service_.get());
  user_prefs::UserPrefs::Set(this, user_pref_service_.get());

  // This constructor is entered during the creation of ContentBrowserClient,
  // before browser threads are created. Therefore any checks to enforce
  // threading (such as BrowserThread::CurrentlyOn()) will fail here.
}

AwBrowserContext::~AwBrowserContext() {
  DCHECK_EQ(this, g_browser_context);
  SimpleKeyMap::GetInstance()->Dissociate(this);
  g_browser_context = NULL;
}

// static
AwBrowserContext* AwBrowserContext::GetDefault() {
  // TODO(joth): rather than store in a global here, lookup this instance
  // from the Java-side peer.
  return g_browser_context;
}

// static
AwBrowserContext* AwBrowserContext::FromWebContents(
    content::WebContents* web_contents) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<AwBrowserContext*>(web_contents->GetBrowserContext());
}

// static
base::FilePath AwBrowserContext::GetCacheDir() {
  FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    NOTREACHED() << "Failed to get app cache directory for Android WebView";
  }
  cache_path =
      cache_path.Append(FILE_PATH_LITERAL("org.chromium.android_webview"));
  return cache_path;
}

// static
base::FilePath AwBrowserContext::GetCookieStorePath() {
  FilePath cookie_store_path;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &cookie_store_path)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  cookie_store_path = cookie_store_path.Append(FILE_PATH_LITERAL("Cookies"));
  return cookie_store_path;
}

// static
void AwBrowserContext::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
}

// static
std::vector<std::string> AwBrowserContext::GetAuthSchemes() {
  // In Chrome this is configurable via the AuthSchemes policy. For WebView
  // there is no interest to have it available so far.
  std::vector<std::string> supported_schemes = {"basic", "digest", "ntlm",
                                                "negotiate"};
  return supported_schemes;
}

void AwBrowserContext::PreMainMessageLoopRun(net::NetLog* net_log) {
  FilePath cache_path = GetCacheDir();

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    url_request_context_getter_ =
        new AwURLRequestContextGetter(cache_path, CreateProxyConfigService(),
                                      user_pref_service_.get(), net_log);
  }

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(this, this, false));
  visitedlink_master_->Init();

  form_database_service_.reset(
      new AwFormDatabaseService(context_storage_path_));

  EnsureResourceContextInitialized(this);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    auto auth_pref_callback = base::BindRepeating(
        &AwBrowserContext::OnAuthPrefsChanged, base::Unretained(this));
    pref_change_registrar_.Add(prefs::kAuthServerWhitelist, auth_pref_callback);
    pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                               auth_pref_callback);
  }

  safe_browsing_ui_manager_ = new AwSafeBrowsingUIManager(
      GetAwURLRequestContext(), user_pref_service_.get());
  safe_browsing_db_manager_ =
      new safe_browsing::RemoteSafeBrowsingDatabaseManager();
  safe_browsing_trigger_manager_ =
      std::make_unique<safe_browsing::TriggerManager>(
          safe_browsing_ui_manager_.get(),
          /*referrer_chain_provider=*/nullptr,
          /*local_state_prefs=*/nullptr);
  safe_browsing_whitelist_manager_ = CreateSafeBrowsingWhitelistManager();

  content::WebUIControllerFactory::RegisterFactory(
      AwWebUIControllerFactory::GetInstance());
}

void AwBrowserContext::OnAuthPrefsChanged() {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams());
}

void AwBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  DCHECK(visitedlink_master_);
  visitedlink_master_->AddURLs(urls);
}

AwQuotaManagerBridge* AwBrowserContext::GetQuotaManagerBridge() {
  if (!quota_manager_bridge_.get()) {
    quota_manager_bridge_ = AwQuotaManagerBridge::Create(this);
  }
  return quota_manager_bridge_.get();
}

AwFormDatabaseService* AwBrowserContext::GetFormDatabaseService() {
  return form_database_service_.get();
}

AwURLRequestContextGetter* AwBrowserContext::GetAwURLRequestContext() {
  return url_request_context_getter_.get();
}

autofill::AutocompleteHistoryManager*
AwBrowserContext::GetAutocompleteHistoryManager() {
  if (!autocomplete_history_manager_) {
    autocomplete_history_manager_ =
        std::make_unique<autofill::AutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(
        form_database_service_->get_autofill_webdata_service(),
        user_pref_service_.get(), IsOffTheRecord());
  }

  return autocomplete_history_manager_.get();
}

base::FilePath AwBrowserContext::GetPath() const {
  return context_storage_path_;
}

bool AwBrowserContext::IsOffTheRecord() const {
  // Android WebView does not support off the record profile yet.
  return false;
}

content::ResourceContext* AwBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_.reset(new AwResourceContext);
  }
  return resource_context_.get();
}

content::DownloadManagerDelegate*
AwBrowserContext::GetDownloadManagerDelegate() {
  if (!GetUserData(kDownloadManagerDelegateKey)) {
    SetUserData(kDownloadManagerDelegateKey,
                std::make_unique<AwDownloadManagerDelegate>());
  }
  return static_cast<AwDownloadManagerDelegate*>(
      GetUserData(kDownloadManagerDelegateKey));
}

content::BrowserPluginGuestManager* AwBrowserContext::GetGuestManager() {
  return NULL;
}

storage::SpecialStoragePolicy* AwBrowserContext::GetSpecialStoragePolicy() {
  // Intentionally returning NULL as 'Extensions' and 'Apps' not supported.
  return NULL;
}

content::PushMessagingService* AwBrowserContext::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in WebView.
  return NULL;
}

content::SSLHostStateDelegate* AwBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get()) {
    ssl_host_state_delegate_.reset(new AwSSLHostStateDelegate());
  }
  return ssl_host_state_delegate_.get();
}

content::PermissionControllerDelegate*
AwBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_.reset(new AwPermissionManager());
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
AwBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
AwBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AwBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AwBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* AwBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  // This function cannot actually create the request context because
  // there is a reentrant dependency on GetResourceContext() via
  // content::StoragePartitionImplMap::Create(). This is not fixable
  // until http://crbug.com/159193. Until then, assert that the context
  // has already been allocated and just handle setting the protocol_handlers.
  DCHECK(url_request_context_getter_.get());
  url_request_context_getter_->SetHandlersAndInterceptors(
      protocol_handlers, std::move(request_interceptors));
  return url_request_context_getter_.get();
}

net::URLRequestContextGetter* AwBrowserContext::CreateMediaRequestContext() {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  return url_request_context_getter_.get();
}

download::InProgressDownloadManager*
AwBrowserContext::RetriveInProgressDownloadManager() {
  return new download::InProgressDownloadManager(
      nullptr, base::FilePath(),
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe), nullptr);
}

AwSafeBrowsingUIManager* AwBrowserContext::GetSafeBrowsingUIManager() const {
  return safe_browsing_ui_manager_.get();
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
AwBrowserContext::GetSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!safe_browsing_db_manager_started_) {
    // V4ProtocolConfig is not used. Just create one with empty values..
    safe_browsing::V4ProtocolConfig config("", false, "", "");
    safe_browsing_db_manager_->StartOnIOThread(
        safe_browsing_ui_manager_->GetURLLoaderFactoryOnIOThread(), config);
    safe_browsing_db_manager_started_ = true;
  }
  return safe_browsing_db_manager_.get();
}

safe_browsing::TriggerManager* AwBrowserContext::GetSafeBrowsingTriggerManager()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return safe_browsing_trigger_manager_.get();
}

AwSafeBrowsingWhitelistManager*
AwBrowserContext::GetSafeBrowsingWhitelistManager() const {
  // Should not be called until the end of PreMainMessageLoopRun,
  return safe_browsing_whitelist_manager_.get();
}

network::mojom::HttpAuthDynamicParamsPtr
AwBrowserContext::CreateHttpAuthDynamicParams() {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->server_whitelist =
      user_pref_service_->GetString(prefs::kAuthServerWhitelist);
  auth_dynamic_params->android_negotiate_account_type =
      user_pref_service_->GetString(prefs::kAuthAndroidNegotiateAccountType);

  return auth_dynamic_params;
}

void AwBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Android WebView rebuilds from WebChromeClient.getVisitedHistory. The client
  // can change in the lifetime of this WebView and may not yet be set here.
  // Therefore this initialization path is not used.
  enumerator->OnComplete(true);
}

}  // namespace android_webview
