// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/test_service.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_quota_permission_context.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/blink_test.mojom.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom.h"
#include "content/shell/common/web_test/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/ssl/client_cert_identity.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_CHROMEOS)
#include "content/public/browser/context_factory.h"
#endif

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

namespace content {

namespace {

ShellContentBrowserClient* g_browser_client;

#if defined(OS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif defined(OS_LINUX)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}
#endif

const service_manager::Manifest& GetContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability(
              "renderer",
              service_manager::Manifest::InterfaceList<
                  mojom::MojoWebTestHelper, mojom::FakeBluetoothChooser,
                  mojom::FakeBluetoothChooserFactory,
                  mojom::WebTestBluetoothFakeAdapterSetter,
                  bluetooth::mojom::FakeBluetooth>())
          .Build()};
  return *manifest;
}

}  // namespace

std::string GetShellUserAgent() {
  std::string product = "Chrome/" CONTENT_SHELL_VERSION;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (base::FeatureList::IsEnabled(blink::features::kFreezeUserAgent)) {
    return content::GetFrozenUserAgent(
               command_line->HasSwitch(switches::kUseMobileUserAgent))
        .as_string();
  }
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
  return BuildUserAgentFromProduct(product);
}

std::string GetShellLanguage() {
  return "en-us,en";
}

blink::UserAgentMetadata GetShellUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand = "content_shell";
  metadata.full_version = CONTENT_SHELL_VERSION;
  metadata.major_version = CONTENT_SHELL_MAJOR_VERSION;
  metadata.platform = BuildOSCpuInfo(false);
  metadata.architecture = BuildCpuInfo();
  metadata.model = BuildModelInfo();

  return metadata;
}

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  return g_browser_client;
}

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(nullptr) {
  DCHECK(!g_browser_client);
  g_browser_client = this;
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  g_browser_client = nullptr;
}

std::unique_ptr<BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  auto browser_main_parts = std::make_unique<ShellBrowserMainParts>(parameters);

  shell_browser_main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  static const char* const kProtocolList[] = {
      url::kHttpScheme, url::kHttpsScheme,        url::kWsScheme,
      url::kWssScheme,  url::kBlobScheme,         url::kFileSystemScheme,
      kChromeUIScheme,  kChromeUIUntrustedScheme, kChromeDevToolsScheme,
      url::kDataScheme, url::kFileScheme,
  };
  for (const char* supported_protocol : kProtocolList) {
    if (url.scheme_piece() == supported_protocol)
      return true;
  }
  return false;
}

bool ShellContentBrowserClient::ShouldTerminateOnServiceQuit(
    const service_manager::Identity& id) {
  if (should_terminate_on_service_quit_callback_)
    return std::move(should_terminate_on_service_quit_callback_).Run(id);
  return false;
}

base::Optional<service_manager::Manifest>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetContentBrowserOverlayManifest();

  return base::nullopt;
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    command_line->AppendSwitch(switches::kEnableCrashReporter);
#if defined(OS_LINUX)
    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
#endif  // OS_LINUX
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir)) {
    command_line->AppendSwitchPath(
        switches::kCrashDumpsDir,
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kCrashDumpsDir));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRegisterFontFiles)) {
    command_line->AppendSwitchASCII(
        switches::kRegisterFontFiles,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRegisterFontFiles));
  }

#if defined(OS_MACOSX)
  // Needed since on Mac, content_browsertests doesn't use
  // content_test_launcher.cc and instead uses shell_main.cc. So give a signal
  // to shell_main.cc that it's a browser test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBrowserTest)) {
    command_line->AppendSwitch(switches::kBrowserTest);
  }
#endif
}

std::string ShellContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return GetShellLanguage();
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

WebContentsViewDelegate* ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return CreateShellWebContentsViewDelegate(web_contents);
}

scoped_refptr<content::QuotaPermissionContext>
ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return new ShellQuotaPermissionContext();
}

GeneratedCodeCacheSettings
ShellContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

base::OnceClosure ShellContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
  if (select_client_certificate_callback_)
    std::move(select_client_certificate_callback_).Run();
  return base::OnceClosure();
}

SpeechRecognitionManagerDelegate*
    ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ShellSpeechRecognitionManagerDelegate();
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    WebPreferences* prefs) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::PreferredColorScheme::kLight;
  }
}

base::FilePath ShellContentBrowserClient::GetFontLookupTableCacheDir() {
  return browser_context()->GetPath().Append(
      FILE_PATH_LITERAL("FontLookupTableCache"));
}

DevToolsManagerDelegate*
ShellContentBrowserClient::GetDevToolsManagerDelegate() {
  return new ShellDevToolsManagerDelegate(browser_context());
}

mojo::Remote<::media::mojom::MediaService>
ShellContentBrowserClient::RunSecondaryMediaService() {
  mojo::Remote<::media::mojom::MediaService> remote;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<::media::MediaService>>>
      service;
  service->emplace(::media::CreateMediaServiceForTesting(
      remote.BindNewPipeAndPassReceiver()));
#endif
  return remote;
}

void ShellContentBrowserClient::OpenURL(
    SiteInstance* site_instance,
    const OpenURLParams& params,
    base::OnceCallback<void(WebContents*)> callback) {
  std::move(callback).Run(
      Shell::CreateNewWindow(site_instance->GetBrowserContext(), params.url,
                             nullptr, gfx::Size())
          ->web_contents());
}

std::unique_ptr<LoginDelegate> ShellContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  if (!login_request_callback_.is_null()) {
    std::move(login_request_callback_).Run(is_main_frame);
  }
  return nullptr;
}

base::FilePath
ShellContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return browser_context()->GetPath();
}

std::string ShellContentBrowserClient::GetUserAgent() {
  return GetShellUserAgent();
}

blink::UserAgentMetadata ShellContentBrowserClient::GetUserAgentMetadata() {
  return GetShellUserAgentMetadata();
}

void ShellContentBrowserClient::OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (url_loader_factory_params_callback_) {
    url_loader_factory_params_callback_.Run(factory_params, origin,
                                            is_for_isolated_world);
  }
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
#endif
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
bool ShellContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                                 RendererSpawnFlags flags) {
  // Add sideloaded font files for testing. See also DIR_WINDOWS_FONTS
  // addition in |StartSandboxedProcess|.
  std::vector<std::string> font_files = switches::GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(font_files.begin());
      i != font_files.end();
      ++i) {
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
        sandbox::TargetPolicy::FILES_ALLOW_READONLY,
        base::UTF8ToWide(*i).c_str());
  }
  return true;
}
#endif  // OS_WIN

mojo::Remote<network::mojom::NetworkContext>
ShellContentBrowserClient::CreateNetworkContext(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  UpdateCorsExemptHeader(context_params.get());
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = GetAcceptLangs(context);

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRunWebTests)) {
    // Configure the Reporting service in a manner expected by certain Web
    // Platform Tests (network-error-logging and reporting-api).
    //
    //   (1) Always send reports (irrespective of BACKGROUND_SYNC permission)
    //   (2) Lower the timeout for sending reports.
    context_params->reporting_delivery_interval =
        kReportingDeliveryIntervalTimeForWebTests;
    context_params->skip_reporting_send_permission_check = true;
  }
#endif

  GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  return network_context;
}

std::vector<base::FilePath>
ShellContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {browser_context()->GetPath()};
}

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

ShellBrowserContext*
    ShellContentBrowserClient::off_the_record_browser_context() {
  return shell_browser_main_parts_->off_the_record_browser_context();
}

}  // namespace content
