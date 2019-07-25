// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_content_browser_overlay_manifest.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "chrome/common/cache_stats_recorder.mojom.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "chrome/common/prerender.mojom.h"
#include "chrome/test/data/webui/web_ui_test.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector.mojom.h"
#include "components/rappor/public/mojom/rappor_recorder.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "components/translate/content/common/translate.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/identity/public/cpp/manifest.h"
#include "services/image_annotation/public/cpp/manifest.h"
#include "services/image_annotation/public/mojom/constants.mojom.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/preferences/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "third_party/blink/public/mojom/input/input_host.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "chrome/services/cups_proxy/public/cpp/manifest.h"
#include "chrome/services/cups_proxy/public/mojom/constants.mojom.h"
#include "chromeos/assistant/buildflags.h"  // nogncheck
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/services/device_sync/public/cpp/manifest.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "chromeos/services/multidevice_setup/public/cpp/manifest.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"  // nogncheck
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
#include "chromeos/services/assistant/public/cpp/manifest.h"  // nogncheck
#endif
#endif

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#else
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/services/app_service/public/cpp/manifest.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/performance_manager/webui_graph_dump.mojom.h"  // nogncheck
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/api/mime_handler.mojom.h"  // nogncheck
#include "extensions/common/mojom/keep_alive.mojom.h"  // nogncheck
#endif

#if defined(BROWSER_MEDIA_CONTROLS_MENU)
#include "third_party/blink/public/mojom/media_controls/touchless/media_controls.mojom.h"
#endif

#if defined(ENABLE_SPATIAL_NAVIGATION_HOST)
#include "third_party/blink/public/mojom/page/spatial_navigation.mojom.h"
#endif

const service_manager::Manifest& GetChromeContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("gpu",
                          service_manager::Manifest::InterfaceList<
                              metrics::mojom::CallStackProfileCollector>())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
                              chrome::mojom::AvailableOfflineContentProvider,
                              chrome::mojom::CacheStatsRecorder,
                              chrome::mojom::NetBenchmarking,
                              data_reduction_proxy::mojom::DataReductionProxy,
                              metrics::mojom::CallStackProfileCollector,
#if defined(OS_WIN)
                              mojom::ModuleEventSink,
#endif
                              rappor::mojom::RapporRecorder,
                              safe_browsing::mojom::SafeBrowsing>())
        .RequireCapability("apps", "app_service")
        .RequireCapability("ash", "system_ui")
        .RequireCapability("ash", "test")
        .RequireCapability("ash", "display")
        .RequireCapability("assistant", "assistant")
        // Only used in the classic Ash case
        .RequireCapability("chrome", "input_device_controller")
        .RequireCapability("chrome_printing", "converter")
        .RequireCapability("cups_ipp_parser", "ipp_parser")
        .RequireCapability("device", "device:fingerprint")
        .RequireCapability("device", "device:geolocation_config")
        .RequireCapability("device", "device:geolocation_control")
        .RequireCapability("device", "device:ip_geolocator")
        .RequireCapability("device_sync", "device_sync")
        .RequireCapability("file_util", "analyze_archive")
        .RequireCapability("file_util", "zip_file")
        .RequireCapability("heap_profiling", "heap_profiler")
        .RequireCapability("heap_profiling", "profiling")
        .RequireCapability("identity", "identity_accessor")
        .RequireCapability(image_annotation::mojom::kServiceName,
                           image_annotation::mojom::kAnnotationCapability)
        .RequireCapability("ime", "input_engine")
        .RequireCapability("media_gallery_util", "parse_media")
        .RequireCapability("mirroring", "mirroring")
        .RequireCapability("nacl_broker", "browser")
        .RequireCapability("nacl_loader", "browser")
        .RequireCapability("noop", "noop")
        .RequireCapability("patch", "patch_file")
        .RequireCapability("pdf_compositor", "compositor")
        .RequireCapability("preferences", "pref_client")
        .RequireCapability("preferences", "pref_control")
        .RequireCapability("profile_import", "import")
        .RequireCapability(quarantine::mojom::kServiceName,
                           quarantine::mojom::kQuarantineFileCapability)
        .RequireCapability("removable_storage_writer",
                           "removable_storage_writer")
        .RequireCapability("secure_channel", "secure_channel")
        .RequireCapability("ui", "ime_registrar")
        .RequireCapability("ui", "input_device_controller")
        .RequireCapability("ui", "window_manager")
        .RequireCapability("unzip", "unzip_file")
        .RequireCapability("util_win", "util_win")
        .RequireCapability("wifi_util_win", "wifi_credentials")
        .RequireCapability("xr_device_service", "xr_device_provider")
        .RequireCapability("xr_device_service", "xr_device_test_hook")
#if defined(OS_CHROMEOS)
        .RequireCapability(
            chromeos::network_config::mojom::kServiceName,
            chromeos::network_config::mojom::kNetworkConfigCapability)
        .RequireCapability(
            chromeos::printing::mojom::kCupsProxyServiceName,
            chromeos::printing::mojom::kStartCupsProxyServiceCapability)
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame",
            chromeos::network_config::mojom::kNetworkConfigCapability,
            service_manager::Manifest::InterfaceList<
                chromeos::network_config::mojom::CrosNetworkConfig>())
        .RequireCapability("cellular_setup", "cellular_setup")
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "cellular_setup",
            service_manager::Manifest::InterfaceList<
                chromeos::cellular_setup::mojom::CellularSetup>())
        .RequireCapability("multidevice_setup", "multidevice_setup")
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "multidevice_setup",
            service_manager::Manifest::InterfaceList<
                chromeos::multidevice_setup::mojom::MultiDeviceSetup,
                chromeos::multidevice_setup::mojom::
                    PrivilegedHostDeviceSetter>())
#endif
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "renderer",
            service_manager::Manifest::InterfaceList<
                autofill::mojom::AutofillDriver,
                autofill::mojom::PasswordManagerDriver,
                blink::mojom::BadgeService, blink::mojom::InstalledAppProvider,
#if defined(BROWSER_MEDIA_CONTROLS_MENU)
                blink::mojom::MediaControlsMenuHost,
#endif
                blink::mojom::ShareService,
#if defined(ENABLE_SPATIAL_NAVIGATION_HOST)
                blink::mojom::SpatialNavigationHost,
#endif
                blink::mojom::TextSuggestionHost,
                chrome::mojom::OfflinePageAutoFetcher,
                chrome::mojom::PrerenderCanceler,
#if defined(OS_CHROMEOS)
                chromeos::ime::mojom::InputEngineManager,
                chromeos::machine_learning::mojom::PageHandler,
                chromeos::media_perception::mojom::MediaPerception,
                cros::mojom::CrosImageCapture,
#endif
                contextual_search::mojom::ContextualSearchJsApiService,
                dom_distiller::mojom::DistillabilityService,
                dom_distiller::mojom::DistillerJavaScriptService,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                extensions::KeepAlive,
                extensions::mime_handler::BeforeUnloadControl,
                extensions::mime_handler::MimeHandlerService,
#endif
                image_annotation::mojom::Annotator,
                media::mojom::MediaEngagementScoreDetailsProvider,
                media_router::mojom::MediaRouter,
                page_load_metrics::mojom::PageLoadMetrics,
                translate::mojom::ContentTranslateDriver,

                // WebUI-only interfaces go below this line. These should be
                // brokered through a dedicated interface, but they're here
                // for for now.
                downloads::mojom::PageHandlerFactory,
                feed_internals::mojom::PageHandler,
#if defined(OS_ANDROID)
                explore_sites_internals::mojom::PageHandler,
#else
                app_management::mojom::PageHandlerFactory,
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
                mojom::DiscardsDetailsProvider,
                performance_manager::mojom::WebUIGraphDump,
#endif
#if defined(OS_CHROMEOS)
                add_supervision::mojom::AddSupervisionHandler,
#endif
                mojom::BluetoothInternalsHandler,
                mojom::InterventionsInternalsPageHandler,
                mojom::OmniboxPageHandler, mojom::ResetPasswordHandler,
                mojom::SiteEngagementDetailsProvider,
                mojom::UsbInternalsPageHandler,
                snippets_internals::mojom::PageHandlerFactory,
                web_ui_test::mojom::TestRunner>())
        .PackageService(identity::GetManifest())
        .PackageService(image_annotation::GetManifest())
        .PackageService(prefs::GetManifest())
#if defined(OS_CHROMEOS)
        .PackageService(chromeos::device_sync::GetManifest())
        .PackageService(chromeos::multidevice_setup::GetManifest())
        .PackageService(chromeos::printing::GetCupsProxyManifest())
#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
        .PackageService(chromeos::assistant::GetManifest())
#endif
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_ANDROID)
        .PackageService(apps::GetManifest())
#endif
        .Build()
  };
  return *manifest;
}
