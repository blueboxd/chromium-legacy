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
#include "android_webview/browser/net/aw_network_delegate.h"
#include "android_webview/browser/net/aw_request_interceptor.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
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


const char kProxyServerSwitch[] = "proxy-server";
const char kProxyBypassListSwitch[] = "proxy-bypass-list";

std::string GetCmdlineOverridesForHostResolver() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.GetSwitchValueASCII(
      network::switches::kHostResolverRules);
}

void ApplyCmdlineOverridesToNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  int value;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    base::StringToInt(command_line.GetSwitchValueASCII(
        switches::kTestingFixedHttpPort), &value);
    params->testing_fixed_http_port = value;
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    base::StringToInt(command_line.GetSwitchValueASCII(
        switches::kTestingFixedHttpsPort), &value);
    params->testing_fixed_https_port = value;
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors)) {
    params->ignore_certificate_errors = true;
  }
}

std::unique_ptr<net::URLRequestJobFactory> CreateJobFactory(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  std::unique_ptr<AwURLRequestJobFactory> aw_job_factory(
      new AwURLRequestJobFactory);
  // Note that the registered schemes must also be specified in
  // AwContentBrowserClient::IsHandledURL.
  bool set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileScheme,
      std::make_unique<net::FileProtocolHandler>(
          base::CreateTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kDataScheme, std::make_unique<net::DataProtocolHandler>());
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kBlobScheme,
      base::WrapUnique((*protocol_handlers)[url::kBlobScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      url::kFileSystemScheme,
      base::WrapUnique((*protocol_handlers)[url::kFileSystemScheme].release()));
  DCHECK(set_protocol);
  set_protocol = aw_job_factory->SetProtocolHandler(
      content::kChromeUIScheme,
      base::WrapUnique(
          (*protocol_handlers)[content::kChromeUIScheme].release()));
  DCHECK(set_protocol);
  protocol_handlers->clear();

  // Note that even though the content:// scheme handler is created here,
  // it cannot be used by child processes until access to it is granted via
  // ChildProcessSecurityPolicy::GrantRequestScheme(). This is done in
  // AwContentBrowserClient.
  request_interceptors.push_back(CreateAndroidContentRequestInterceptor());
  request_interceptors.push_back(CreateAndroidAssetFileRequestInterceptor());
  // The AwRequestInterceptor must come after the content and asset file job
  // factories. This for WebViewClassic compatibility where it was not
  // possible to intercept resource loads to resolvable content:// and
  // file:// URIs.
  // This logical dependency is also the reason why the Content
  // URLRequestInterceptor has to be added as an interceptor rather than as a
  // ProtocolHandler.
  request_interceptors.push_back(std::make_unique<AwRequestInterceptor>());

  // The chain of responsibility will execute the handlers in reverse to the
  // order in which the elements of the chain are created.
  std::unique_ptr<net::URLRequestJobFactory> job_factory(
      std::move(aw_job_factory));
  for (auto i = request_interceptors.rbegin(); i != request_interceptors.rend();
       ++i) {
    job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory), std::move(*i)));
  }
  request_interceptors.clear();

  return job_factory;
}

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
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  auth_server_whitelist_.Init(
      prefs::kAuthServerWhitelist, user_pref_service,
      base::BindRepeating(&AwURLRequestContextGetter::UpdateServerWhitelist,
                          base::Unretained(this)));
  auth_server_whitelist_.MoveToSequence(io_thread_proxy);

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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!url_request_context_);
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));

  net::URLRequestContextBuilder builder;

  builder.set_network_delegate(std::make_unique<AwNetworkDelegate>());
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder.set_ftp_enabled(false);  // Android WebView does not support ftp yet.
#endif
  DCHECK(proxy_config_service_.get());

  // Android provides a local HTTP proxy that handles all the proxying.
  // Create the proxy without a resolver since we rely on this local HTTP proxy.
  // TODO(sgurun) is this behavior guaranteed through SDK?

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kProxyServerSwitch)) {
    std::string proxy = command_line.GetSwitchValueASCII(kProxyServerSwitch);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy);
    if (command_line.HasSwitch(kProxyBypassListSwitch)) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII(kProxyBypassListSwitch);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }

    builder.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateFixed(net::ProxyConfigWithAnnotation(
            proxy_config, NO_TRAFFIC_ANNOTATION_YET)));
  } else {
    builder.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateWithoutProxyResolver(
            std::move(proxy_config_service_), net_log_));
  }
  builder.set_net_log(net_log_);
  builder.SetCookieStore(std::make_unique<AwCookieStoreWrapper>());

  net::URLRequestContextBuilder::HttpCacheParams cache_params;
  cache_params.type =
      net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE;
  cache_params.max_size = GetHttpCacheSize();
  cache_params.path = cache_path_;
  builder.EnableHttpCache(cache_params);

  net::HttpNetworkSession::Params network_session_params;
  ApplyCmdlineOverridesToNetworkSessionParams(&network_session_params);
  builder.set_http_network_session_params(network_session_params);

  // Quic is only supported when network service is enabled.
  builder.SetSpdyAndQuicEnabled(true, false);

  builder.SetHttpAuthHandlerFactory(CreateAuthHandlerFactory());
  builder.set_host_mapping_rules(GetCmdlineOverridesForHostResolver());

  // Context copy allowed because NetworkService is confirmed disabled.
  builder.set_allow_copy();

  builder.set_enable_brotli(base::FeatureList::IsEnabled(
      android_webview::features::kWebViewBrotliSupport));

  url_request_context_ = builder.Build();

  // For Android WebView, do not enforce policies that are not consistent with
  // the underlying OS validator.
  // This means not enforcing the Legacy Symantec PKI policies outlined in
  // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
  // or disabling SHA-1 for locally-installed trust anchors.
  net::CertVerifier::Config config;
  config.enable_sha1_local_anchors = true;
  config.disable_symantec_enforcement = true;
  url_request_context_->cert_verifier()->SetConfig(config);

#if DCHECK_IS_ON()
  g_created_url_request_context_builder = true;
#endif
  url_request_context_->set_check_cleartext_permitted(
    g_check_cleartext_permitted);

  job_factory_ =
      CreateJobFactory(&protocol_handlers_, std::move(request_interceptors_));
  url_request_context_->set_job_factory(job_factory_.get());
  url_request_context_->set_http_user_agent_settings(
      http_user_agent_settings_.get());
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
  return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
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
  UpdateServerWhitelist();
  UpdateAndroidAuthNegotiateAccountType();

  return net::HttpAuthHandlerRegistryFactory::Create(
      http_auth_preferences_.get(), AwBrowserContext::GetAuthSchemes());
}

void AwURLRequestContextGetter::UpdateServerWhitelist() {
  http_auth_preferences_->SetServerWhitelist(auth_server_whitelist_.GetValue());
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
