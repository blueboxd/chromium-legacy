// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/base_paths_posix.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/features.h"

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
AwBrowserProcess* g_aw_browser_process = nullptr;
}  // namespace

// static
AwBrowserProcess* AwBrowserProcess::GetInstance() {
  return g_aw_browser_process;
}

AwBrowserProcess::AwBrowserProcess(
    AwFeatureListCreator* aw_feature_list_creator) {
  g_aw_browser_process = this;
  aw_feature_list_creator_ = aw_feature_list_creator;
}

AwBrowserProcess::~AwBrowserProcess() {
  g_aw_browser_process = nullptr;
}

void AwBrowserProcess::PreMainMessageLoopRun() {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    pref_change_registrar_.Init(local_state());
    auto auth_pref_callback = base::BindRepeating(
        &AwBrowserProcess::OnAuthPrefsChanged, base::Unretained(this));
    pref_change_registrar_.Add(prefs::kAuthServerWhitelist, auth_pref_callback);
    pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                               auth_pref_callback);
  }

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    CreateURLRequestContextGetter();
  }
  InitSafeBrowsing();
}

PrefService* AwBrowserProcess::local_state() {
  if (!local_state_)
    CreateLocalState();
  return local_state_.get();
}

void AwBrowserProcess::CreateLocalState() {
  DCHECK(!local_state_);

  local_state_ = aw_feature_list_creator_->TakePrefService();
  DCHECK(local_state_);
}

void AwBrowserProcess::InitSafeBrowsing() {
  CreateSafeBrowsingUIManager();
  CreateSafeBrowsingWhitelistManager();
}

void AwBrowserProcess::CreateSafeBrowsingUIManager() {
  safe_browsing_ui_manager_ =
      new AwSafeBrowsingUIManager(AwBrowserProcess::GetAwURLRequestContext());
}

void AwBrowserProcess::CreateSafeBrowsingWhitelistManager() {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  safe_browsing_whitelist_manager_ =
      std::make_unique<AwSafeBrowsingWhitelistManager>(background_task_runner,
                                                       io_task_runner);
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
AwBrowserProcess::GetSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_db_manager_) {
    safe_browsing_db_manager_ =
        new safe_browsing::RemoteSafeBrowsingDatabaseManager();
  }

  if (!safe_browsing_db_manager_started_) {
    // V4ProtocolConfig is not used. Just create one with empty values..
    safe_browsing::V4ProtocolConfig config("", false, "", "");
    safe_browsing_db_manager_->StartOnIOThread(
        GetSafeBrowsingUIManager()->GetURLLoaderFactoryOnIOThread(), config);
    safe_browsing_db_manager_started_ = true;
  }

  return safe_browsing_db_manager_.get();
}

safe_browsing::TriggerManager*
AwBrowserProcess::GetSafeBrowsingTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!safe_browsing_trigger_manager_) {
    safe_browsing_trigger_manager_ =
        std::make_unique<safe_browsing::TriggerManager>(
            GetSafeBrowsingUIManager(),
            /*referrer_chain_provider=*/nullptr,
            /*local_state_prefs=*/nullptr);
  }

  return safe_browsing_trigger_manager_.get();
}

AwSafeBrowsingWhitelistManager*
AwBrowserProcess::GetSafeBrowsingWhitelistManager() const {
  return safe_browsing_whitelist_manager_.get();
}

AwSafeBrowsingUIManager* AwBrowserProcess::GetSafeBrowsingUIManager() const {
  return safe_browsing_ui_manager_.get();
}

// static
void AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  pref_registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                                    std::string());
}

network::mojom::HttpAuthDynamicParamsPtr
AwBrowserProcess::CreateHttpAuthDynamicParams() {
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->server_whitelist =
      local_state()->GetString(prefs::kAuthServerWhitelist);
  auth_dynamic_params->android_negotiate_account_type =
      local_state()->GetString(prefs::kAuthAndroidNegotiateAccountType);

  return auth_dynamic_params;
}

void AwBrowserProcess::OnAuthPrefsChanged() {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams());
}

namespace {
std::unique_ptr<net::ProxyConfigServiceAndroid> CreateProxyConfigService() {
  std::unique_ptr<net::ProxyConfigServiceAndroid> config_service_android =
      std::make_unique<net::ProxyConfigServiceAndroid>(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
          base::ThreadTaskRunnerHandle::Get());

  config_service_android->set_exclude_pac_url(true);
  return config_service_android;
}
}  // namespace

// Default profile reuses global URLRequestGetter
void AwBrowserProcess::CreateURLRequestContextGetter() {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  base::FilePath cache_path;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_path)) {
    NOTREACHED() << "Failed to get app cache directory for Android WebView";
  }
  cache_path =
      cache_path.Append(FILE_PATH_LITERAL("org.chromium.android_webview"));

  url_request_context_getter_ = new AwURLRequestContextGetter(
      cache_path, CreateProxyConfigService(), local_state(), new net::NetLog());
}

AwURLRequestContextGetter* AwBrowserProcess::GetAwURLRequestContext() {
  return url_request_context_getter_.get();
}

}  // namespace android_webview
