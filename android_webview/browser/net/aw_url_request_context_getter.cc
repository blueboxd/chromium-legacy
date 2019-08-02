// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_url_request_context_getter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list.h"
#include "android_webview/browser/net/aw_cookie_store_wrapper.h"
#include "android_webview/browser/net/aw_http_user_agent_settings.h"
#include "android_webview/browser/net/init_native_callback.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/common/aw_content_client.h"
#include "base/base_paths_android.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/cache_type.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"

using base::FilePath;
using content::BrowserThread;

namespace android_webview {


namespace {

#if DCHECK_IS_ON()
bool g_created_url_request_context_builder = false;
#endif
// On apps targeting API level O or later, check cleartext is enforced.
bool g_check_cleartext_permitted = false;

}  // namespace

AwURLRequestContextGetter::AwURLRequestContextGetter(
    const base::FilePath& cache_path,
    std::unique_ptr<net::ProxyConfigServiceAndroid> config_service,
    PrefService* user_pref_service,
    net::NetLog* net_log)
    : cache_path_(cache_path),
      net_log_(net_log),
      proxy_config_service_(std::move(config_service)),
      proxy_config_service_android_(proxy_config_service_.get()),
      http_user_agent_settings_(new AwHttpUserAgentSettings()) {
  // CreateSystemProxyConfigService for Android must be called on main thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});

  auth_server_allowlist_.Init(
      prefs::kAuthServerWhitelist, user_pref_service,
      base::BindRepeating(&AwURLRequestContextGetter::UpdateServerAllowlist,
                          base::Unretained(this)));
  auth_server_allowlist_.MoveToSequence(io_thread_proxy);

  auth_android_negotiate_account_type_.Init(
      prefs::kAuthAndroidNegotiateAccountType, user_pref_service,
      base::BindRepeating(
          &AwURLRequestContextGetter::UpdateAndroidAuthNegotiateAccountType,
          base::Unretained(this)));
  auth_android_negotiate_account_type_.MoveToSequence(io_thread_proxy);

  // For net-log, use default capture mode and no channel information.
  // WebView can enable net-log only using commandline in userdebug
  // devices so there is no need to complicate things here. The net_log
  // file is written at an absolute path specified by the user using
  // --log-net-log=<filename.json>. Note: the absolute path should be a
  // subdirectory of the app's data directory, otherwise multiple WebView apps
  // may write to the same file. The user should then 'adb pull' the file to
  // desktop and then import it to chrome://net-internals There is no good way
  // to shutdown net-log at the moment. The file will always be truncated.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // Note: Netlog is handled by the Network Service when that is enabled.
  if (command_line.HasSwitch(network::switches::kLogNetLog) &&
      !base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Assume the user gave us a path we can write to
    FilePath net_log_path =
        command_line.GetSwitchValuePath(network::switches::kLogNetLog);

    std::unique_ptr<base::DictionaryValue> constants_dict =
        net::GetNetConstants();
    // Add a dictionary with client information
    auto dict = std::make_unique<base::DictionaryValue>();

    dict->SetString("name", version_info::GetProductName());
    dict->SetString("version", version_info::GetVersionNumber());
    dict->SetString("cl", version_info::GetLastChange());
    dict->SetString("official", version_info::IsOfficialBuild() ? "official"
                                                                : "unofficial");
    std::string os_type = base::StringPrintf(
        "%s: %s (%s)", base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str());
    dict->SetString("os_type", os_type);

    dict->SetString(
        "command_line",
        base::CommandLine::ForCurrentProcess()->GetCommandLineString());
    constants_dict->Set("clientInfo", std::move(dict));

    file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
        net_log_path, std::move(constants_dict));
    file_net_log_observer_->StartObserving(net_log_,
                                           net::NetLogCaptureMode::kDefault);
  }
}

AwURLRequestContextGetter::~AwURLRequestContextGetter() {
}

void AwURLRequestContextGetter::InitializeURLRequestContext() {
  // network service is enabled by default, this code path is never executed.
  NOTREACHED();
}

// static
void AwURLRequestContextGetter::set_check_cleartext_permitted(bool permitted) {
#if DCHECK_IS_ON()
  DCHECK(!g_created_url_request_context_builder);
#endif
  g_check_cleartext_permitted = permitted;
}

net::URLRequestContext* AwURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!url_request_context_)
    InitializeURLRequestContext();

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
AwURLRequestContextGetter::GetNetworkTaskRunner() const {
  return base::CreateSingleThreadTaskRunner({BrowserThread::IO});
}

void AwURLRequestContextGetter::SetHandlersAndInterceptors(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  std::swap(protocol_handlers_, *protocol_handlers);
  request_interceptors_.swap(request_interceptors);
}

std::unique_ptr<net::HttpAuthHandlerFactory>
AwURLRequestContextGetter::CreateAuthHandlerFactory() {
  http_auth_preferences_.reset(new net::HttpAuthPreferences());
  UpdateServerAllowlist();
  UpdateAndroidAuthNegotiateAccountType();

  return net::HttpAuthHandlerRegistryFactory::Create(
      http_auth_preferences_.get(), AwBrowserContext::GetAuthSchemes());
}

void AwURLRequestContextGetter::UpdateServerAllowlist() {
  http_auth_preferences_->SetServerAllowlist(auth_server_allowlist_.GetValue());
}

void AwURLRequestContextGetter::UpdateAndroidAuthNegotiateAccountType() {
  http_auth_preferences_->set_auth_android_negotiate_account_type(
      auth_android_negotiate_account_type_.GetValue());
}

std::string AwURLRequestContextGetter::SetProxyOverride(
    const std::vector<net::ProxyConfigServiceAndroid::ProxyOverrideRule>&
        proxy_rules,
    const std::vector<std::string>& bypass_rules,
    base::OnceClosure callback) {
  DCHECK(proxy_config_service_android_ != nullptr);
  return proxy_config_service_android_->SetProxyOverride(
      proxy_rules, bypass_rules, std::move(callback));
}

void AwURLRequestContextGetter::ClearProxyOverride(base::OnceClosure callback) {
  DCHECK(proxy_config_service_android_ != nullptr);
  proxy_config_service_android_->ClearProxyOverride(std::move(callback));
}

}  // namespace android_webview
