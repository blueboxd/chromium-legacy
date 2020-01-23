// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_browser_client_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/embedder_support/switches.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "content/public/common/window_container_type.mojom.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/browser_main_parts_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/ssl_error_handler.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/browser/weblayer_browser_interface_binders.h"
#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"
#include "weblayer/common/features.h"
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
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "components/spellcheck/browser/spell_check_host_impl.h"  // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "weblayer/browser/android_descriptors.h"
#include "weblayer/browser/devtools_manager_delegate_android.h"
#include "weblayer/browser/java/jni/ExternalNavigationHandler_jni.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

namespace switches {
// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
// TODO(alexclarke): Find a better place for this.
const char kProxyBypassList[] = "proxy-bypass-list";
}  // namespace switches

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

}  // namespace

namespace weblayer {

ContentBrowserClientImpl::ContentBrowserClientImpl(MainParams* params)
    : params_(params) {}

ContentBrowserClientImpl::~ContentBrowserClientImpl() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<BrowserMainPartsImpl> browser_main_parts =
      std::make_unique<BrowserMainPartsImpl>(params_, parameters);

  return browser_main_parts;
}

void ContentBrowserClientImpl::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebLayerFakePermissions)) {
    command_line->AppendSwitch(switches::kWebLayerFakePermissions);
  }
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
  return nullptr;
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

std::string ContentBrowserClientImpl::GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string ContentBrowserClientImpl::GetUserAgent() {
  std::string product = GetProduct();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
  return content::BuildUserAgentFromProduct(product);
}

blink::UserAgentMetadata ContentBrowserClientImpl::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand = version_info::GetProductName();
  metadata.full_version = version_info::GetVersionNumber();
  metadata.major_version = version_info::GetMajorVersionNumber();
  metadata.platform = version_info::GetOSType();
  metadata.architecture = content::BuildCpuInfo();
  metadata.model = content::BuildModelInfo();

  return metadata;
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return;
  TabImpl* tab = TabImpl::FromWebContents(web_contents);
  prefs->fullscreen_supported = tab && tab->fullscreen_delegate();
}

mojo::Remote<network::mojom::NetworkContext>
ContentBrowserClientImpl::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = GetAcceptLangs(context);
  if (!context->IsOffTheRecord()) {
    base::FilePath cookie_path = context->GetPath();
    cookie_path = cookie_path.Append(FILE_PATH_LITERAL("Cookies"));
    context_params->cookie_path = cookie_path;
    context_params->http_cache_path =
        ProfileImpl::GetCachePath(context).Append(FILE_PATH_LITERAL("Cache"));
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line->GetSwitchValueASCII(switches::kProxyServer);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy_server);
    if (command_line->HasSwitch(switches::kProxyBypassList)) {
      std::string bypass_list =
          command_line->GetSwitchValueASCII(switches::kProxyBypassList);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_config,
        net::DefineNetworkTrafficAnnotation("undefined", "Nothing here yet."));
  }
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

  // Create SystemNetworkContextManager if it has not been created yet. We need
  // to set up global NetworkService state before anything else uses it and this
  // is the first opportunity to initialize SystemNetworkContextManager with the
  // NetworkService.
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(GetUserAgent());

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

  if (container_type == content::mojom::WindowContainerType::BACKGROUND) {
    // TODO(https://crbug.com/1019923): decide if WebLayer needs to support
    // background tabs.
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
  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle, std::make_unique<SSLCertReporterImpl>(),
      base::BindOnce(&HandleSSLError), base::BindOnce(&IsInHostedApp)));
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

void ContentBrowserClientImpl::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateWebLayerFrameBinders(render_frame_host, map);
}

#if defined(OS_ANDROID)
SafeBrowsingService* ContentBrowserClientImpl::GetSafeBrowsingService() {
  if (!safe_browsing_service_) {
    // Create and initialize safe_browsing_service on first get.
    // Note: Initialize() needs to happen on UI thread.
    safe_browsing_service_ =
        std::make_unique<SafeBrowsingService>(GetUserAgent());
    safe_browsing_service_->Initialize();
  }
  return safe_browsing_service_.get();
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

#if defined(OS_ANDROID)
bool ContentBrowserClientImpl::ShouldOverrideUrlLoading(
    int frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  *ignore_navigation = false;

  // Only GETs can be overridden.
  if (request_method != "GET")
    return true;

  bool application_initiated =
      browser_initiated || transition & ui::PAGE_TRANSITION_FORWARD_BACK;

  // Don't offer application-initiated navigations unless it's a redirect.
  if (application_initiated && !is_redirect)
    return true;

  // For HTTP schemes, only top-level navigations can be overridden. Similarly,
  // WebView Classic lets app override only top level about:blank navigations.
  // So we filter out non-top about:blank navigations here.
  if (!is_main_frame &&
      (gurl.SchemeIs(url::kHttpScheme) || gurl.SchemeIs(url::kHttpsScheme) ||
       gurl.SchemeIs(url::kAboutScheme)))
    return true;

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents == nullptr)
    return true;

  JNIEnv* env = base::android::AttachCurrentThread();

  base::string16 url = base::UTF8ToUTF16(gurl.possibly_invalid_spec());
  base::android::ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF16ToJavaString(env, url);

  *ignore_navigation = Java_ExternalNavigationHandler_shouldOverrideUrlLoading(
      env, jurl, has_user_gesture, is_redirect, is_main_frame);

  if (base::android::HasException(env)) {
    // Tell the chromium message loop to not perform any tasks after the
    // current one - we want to make sure we return to Java cleanly without
    // first making any new JNI calls.
    base::MessageLoopCurrentForUI::Get()->Abort();
    // If we crashed we don't want to continue the navigation.
    *ignore_navigation = true;
    return false;
  }

  return true;
}
#endif

}  // namespace weblayer
