// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_browser_client_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/embedder_support/switches.h"
#include "components/network_time/network_time_tracker.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/permissions/quota_permission_context_impl.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/common/window_container_type.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "weblayer/browser/browser_main_parts_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/tab_specific_content_settings_delegate.h"
#include "weblayer/browser/user_agent.h"
#include "weblayer/browser/web_contents_view_delegate_impl.h"
#include "weblayer/browser/weblayer_browser_interface_binders.h"
#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"
#include "weblayer/browser/weblayer_security_blocking_page_factory.h"
#include "weblayer/common/features.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/common/switches.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/public/main.h"

#if defined(OS_ANDROID)
#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"  // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "weblayer/browser/android_descriptors.h"
#include "weblayer/browser/devtools_manager_delegate_android.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "weblayer/browser/captive_portal_service_factory.h"
#endif

namespace switches {
// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
// TODO(alexclarke): Find a better place for this.
const char kProxyBypassList[] = "proxy-bypass-list";
}  // namespace switches

namespace weblayer {

namespace {

bool IsSafebrowsingSupported() {
  // TODO(timvolodine): consider the non-android case, see crbug.com/1015809.
  // TODO(timvolodine): consider refactoring this out into safe_browsing/.
#if defined(OS_ANDROID)
  return true;
#endif
  return false;
}

bool IsInHostedApp(content::WebContents* web_contents) {
  return false;
}

class SSLCertReporterImpl : public SSLCertReporter {
 public:
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {}
};

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //weblayer-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
#endif

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(ssl_cert_reporter), std::move(blocking_page_ready_callback),
      BrowserProcess::GetInstance()->GetNetworkTimeTracker(),
      captive_portal_service,
      std::make_unique<WebLayerSecurityBlockingPageFactory>());
}

#if defined(OS_ANDROID)
void CreateOriginId(cdm::MediaDrmStorageImpl::OriginIdObtainedCB callback) {
  std::move(callback).Run(true, base::UnguessableToken::Create());
}

void AllowEmptyOriginIdCB(base::OnceCallback<void(bool)> callback) {
  // Since CreateOriginId() always returns a non-empty origin ID, we don't need
  // to allow empty origin ID.
  std::move(callback).Run(false);
}

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<::media::mojom::MediaDrmStorage> receiver) {
  DCHECK(render_frame_host);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(
      render_frame_host, base::BindRepeating(&CreateOriginId),
      base::BindRepeating(&AllowEmptyOriginIdCB), std::move(receiver));
}
#endif  // defined(OS_ANDROID)

void RegisterPrefs(PrefRegistrySimple* pref_registry) {
  network_time::NetworkTimeTracker::RegisterPrefs(pref_registry);
  pref_registry->RegisterIntegerPref(kDownloadNextIDPref, 0);
#if defined(OS_ANDROID)
  metrics::AndroidMetricsServiceClient::RegisterPrefs(pref_registry);
#endif
  variations::VariationsService::RegisterPrefs(pref_registry);
}

}  // namespace

ContentBrowserClientImpl::ContentBrowserClientImpl(MainParams* params)
    : params_(params) {
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(GetUserAgent());
}

ContentBrowserClientImpl::~ContentBrowserClientImpl() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  // This should be called after CreateFeatureListAndFieldTrials(), which
  // creates |local_state_|.
  DCHECK(local_state_);
  std::unique_ptr<BrowserMainPartsImpl> browser_main_parts =
      std::make_unique<BrowserMainPartsImpl>(params_, parameters,
                                             std::move(local_state_));

  return browser_main_parts;
}

std::string ContentBrowserClientImpl::GetApplicationLocale() {
  return i18n::GetApplicationLocale();
}

std::string ContentBrowserClientImpl::GetAcceptLangs(
    content::BrowserContext* context) {
  return i18n::GetAcceptLangs();
}

content::WebContentsViewDelegate*
ContentBrowserClientImpl::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new WebContentsViewDelegateImpl(web_contents);
}

bool ContentBrowserClientImpl::CanShutdownGpuProcessNowOnIOThread() {
  return true;
}

content::DevToolsManagerDelegate*
ContentBrowserClientImpl::GetDevToolsManagerDelegate() {
#if defined(OS_ANDROID)
  return new DevToolsManagerDelegateAndroid();
#else
  return new content::DevToolsManagerDelegate();
#endif
}

base::Optional<service_manager::Manifest>
ContentBrowserClientImpl::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetWebLayerContentBrowserOverlayManifest();
  return base::nullopt;
}

void ContentBrowserClientImpl::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  page_load_metrics::mojom::PageLoadFeatures new_features({feature}, {}, {});
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, new_features);
}

std::string ContentBrowserClientImpl::GetProduct() {
  return weblayer::GetProduct();
}

std::string ContentBrowserClientImpl::GetUserAgent() {
  return weblayer::GetUserAgent();
}

blink::UserAgentMetadata ContentBrowserClientImpl::GetUserAgentMetadata() {
  return weblayer::GetUserAgentMetadata();
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  prefs->default_encoding = l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING);

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (tab)
    tab->SetWebPreferences(prefs);
}

mojo::Remote<network::mojom::NetworkContext>
ContentBrowserClientImpl::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      SystemNetworkContextManager::CreateDefaultNetworkContextParams(
          GetUserAgent());
  // Headers coming from the embedder are implicitly trusted and should not
  // trigger CORS checks.
  context_params->allow_any_cors_exempt_header_for_browser = true;
  context_params->accept_language = GetAcceptLangs(context);
  if (!context->IsOffTheRecord()) {
    base::FilePath cookie_path = context->GetPath();
    cookie_path = cookie_path.Append(FILE_PATH_LITERAL("Cookies"));
    context_params->cookie_path = cookie_path;
    context_params->http_cache_path =
        ProfileImpl::GetCachePath(context).Append(FILE_PATH_LITERAL("Cache"));
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kProxyServer)) {
    std::string proxy_server =
        command_line->GetSwitchValueASCII(::switches::kProxyServer);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy_server);
    if (command_line->HasSwitch(::switches::kProxyBypassList)) {
      std::string bypass_list =
          command_line->GetSwitchValueASCII(::switches::kProxyBypassList);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_config,
        net::DefineNetworkTrafficAnnotation("undefined", "Nothing here yet."));
  }
  variations::UpdateCorsExemptHeaderForVariations(context_params.get());
  content::GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  return network_context;
}

void ContentBrowserClientImpl::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  network::mojom::CryptConfigPtr config = network::mojom::CryptConfig::New();
  content::GetNetworkService()->SetCryptConfig(std::move(config));
#endif
  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ContentBrowserClientImpl::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing) &&
      IsSafebrowsingSupported()) {
#if defined(OS_ANDROID)
    result.push_back(GetSafeBrowsingService()->CreateURLLoaderThrottle(
        wc_getter, frame_tree_node_id));
#endif
  }

  return result;
}

bool ContentBrowserClientImpl::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // WebLayer handles error cases.
    return true;
  }

  std::string scheme = url.scheme();

  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  static const char* const kProtocolList[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kWsScheme,
    url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
    url::kFileScheme,
    content::kChromeDevToolsScheme,
    content::kChromeUIScheme,
    content::kChromeUIUntrustedScheme,
    url::kDataScheme,
#if defined(OS_ANDROID)
    url::kContentScheme,
#endif  // defined(OS_ANDROID)
    url::kAboutScheme,
    url::kBlobScheme,
    url::kFileSystemScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (scheme == supported_protocol)
      return true;
  }

#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  if (scheme == url::kFtpScheme &&
      base::FeatureList::IsEnabled(::features::kFtpProtocol)) {
    return true;
  }
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  return false;
}

bool ContentBrowserClientImpl::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);

  // Block popups if there is no NewTabDelegate.
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  if (!tab || !tab->has_new_tab_delegate())
    return false;

  if (container_type == content::mojom::WindowContainerType::BACKGROUND ||
      container_type == content::mojom::WindowContainerType::PERSISTENT) {
    // WebLayer does not support extensions/apps, which are the only permitted
    // users of background windows.
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          embedder_support::kDisablePopupBlocking)) {
    return true;
  }

  // WindowOpenDisposition has a *ton* of types, but the following are really
  // the only ones that should be hit for this code path.
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      FALLTHROUGH;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      FALLTHROUGH;
    case WindowOpenDisposition::NEW_POPUP:
      FALLTHROUGH;
    case WindowOpenDisposition::NEW_WINDOW:
      break;
    default:
      return false;
  }

  // TODO(https://crbug.com/1019922): support proper popup blocking.
  return user_gesture;
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ContentBrowserClientImpl::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  if (handle->IsInMainFrame()) {
    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    throttles.push_back(
        page_load_metrics::MetricsNavigationThrottle::Create(handle));
  }

  // The next highest priority throttle *must* be this as it's responsible for
  // calling to NavigationController for certain events.
  TabImpl* tab = TabImpl::FromWebContents(handle->GetWebContents());
  if (tab) {
    auto throttle =
        static_cast<NavigationControllerImpl*>(tab->GetNavigationController())
            ->CreateNavigationThrottle(handle);
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle, std::make_unique<SSLCertReporterImpl>(),
      base::BindOnce(&HandleSSLErrorWrapper), base::BindOnce(&IsInHostedApp)));

#if defined(OS_ANDROID)
  if (handle->IsInMainFrame()) {
    if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing) &&
        IsSafebrowsingSupported()) {
      throttles.push_back(
          GetSafeBrowsingService()->CreateSafeBrowsingNavigationThrottle(
              handle));
      if (handle->IsInMainFrame()) {
        throttles.push_back(
            navigation_interception::InterceptNavigationDelegate::
                CreateThrottleFor(
                    handle, navigation_interception::SynchronyMode::kAsync));
      }
    }
  }
#endif
  return throttles;
}

content::GeneratedCodeCacheSettings
ContentBrowserClientImpl::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  DCHECK(context);
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(
      true, 0, ProfileImpl::GetCachePath(context));
}

bool ContentBrowserClientImpl::BindAssociatedReceiverFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  if (interface_name == autofill::mojom::AutofillDriver::Name_) {
    autofill::ContentAutofillDriverFactory::BindAutofillDriver(
        mojo::PendingAssociatedReceiver<autofill::mojom::AutofillDriver>(
            std::move(*handle)),
        render_frame_host);
    return true;
  }

  return false;
}

void ContentBrowserClientImpl::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
#if defined(OS_ANDROID)
  auto create_spellcheck_host =
      [](mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        mojo::MakeSelfOwnedReceiver(std::make_unique<SpellCheckHostImpl>(),
                                    std::move(receiver));
      };
  registry->AddInterface(
      base::BindRepeating(create_spellcheck_host),
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}));

  if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing) &&
      IsSafebrowsingSupported()) {
    GetSafeBrowsingService()->AddInterface(registry, render_process_host);
  }
#endif  // defined(OS_ANDROID)
}

void ContentBrowserClientImpl::BindMediaServiceReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
#if defined(OS_ANDROID)
  if (auto r = receiver.As<media::mojom::MediaDrmStorage>()) {
    CreateMediaDrmStorage(render_frame_host, std::move(r));
    return;
  }
#endif
}

void ContentBrowserClientImpl::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateWebLayerFrameBinders(render_frame_host, map);
}

void ContentBrowserClientImpl::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
#if defined(OS_ANDROID)
  host->AddFilter(new cdm::CdmMessageFilterAndroid(
      /*can_persist_data*/ true,
      /*force_to_support_secure_codecs*/ false));
#endif
  TabSpecificContentSettingsDelegate::UpdateRendererContentSettingRules(host);
}

scoped_refptr<content::QuotaPermissionContext>
ContentBrowserClientImpl::CreateQuotaPermissionContext() {
  return base::MakeRefCounted<permissions::QuotaPermissionContextImpl>();
}

void ContentBrowserClientImpl::CreateFeatureListAndFieldTrials() {
  local_state_ = CreateLocalState();
  feature_list_creator_ =
      std::make_unique<FeatureListCreator>(local_state_.get());
  feature_list_creator_->SetSystemNetworkContextManager(
      SystemNetworkContextManager::GetInstance());
  feature_list_creator_->CreateFeatureListAndFieldTrials();
}

#if defined(OS_ANDROID)
SafeBrowsingService* ContentBrowserClientImpl::GetSafeBrowsingService() {
  return BrowserProcess::GetInstance()->GetSafeBrowsingService(GetUserAgent());
}
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
void ContentBrowserClientImpl::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kWebLayerMainPakDescriptor, fd, region);

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kWebLayer100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kWebLayerLocalePakDescriptor, fd, region);

  if (base::android::BundleUtils::IsBundle()) {
    fd = ui::GetSecondaryLocalePackFd(&region);
    mappings->ShareWithRegion(kWebLayerSecondaryLocalePakDescriptor, fd,
                              region);
  } else {
    mappings->ShareWithRegion(kWebLayerSecondaryLocalePakDescriptor,
                              base::GlobalDescriptors::GetInstance()->Get(
                                  kWebLayerSecondaryLocalePakDescriptor),
                              base::GlobalDescriptors::GetInstance()->GetRegion(
                                  kWebLayerSecondaryLocalePakDescriptor));
  }

  int crash_signal_fd =
      crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
  if (crash_signal_fd >= 0)
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
#endif  // defined(OS_ANDROID)
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

void ContentBrowserClientImpl::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine& browser_command_line(
      *base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line->GetSwitchValueASCII(::switches::kProcessType);
  if (process_type == ::switches::kRendererProcess) {
    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
        embedder_support::kOriginTrialDisabledFeatures,
        embedder_support::kOriginTrialDisabledTokens,
        embedder_support::kOriginTrialPublicKey,
    };
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  }
}

// static
std::unique_ptr<PrefService> ContentBrowserClientImpl::CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  RegisterPrefs(pref_registry.get());
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");
  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  {
    // Creating the prefs service may require reading the preferences from
    // disk.
    base::ScopedAllowBlocking allow_io;
    return pref_service_factory.Create(pref_registry);
  }
}

#if defined(OS_ANDROID)
content::ContentBrowserClient::WideColorGamutHeuristic
ContentBrowserClientImpl::GetWideColorGamutHeuristic() {
  // Always match window since a mismatch can cause inefficiency in surface
  // flinger.
  return WideColorGamutHeuristic::kUseWindow;
}
#endif  // OS_ANDROID

}  // namespace weblayer
