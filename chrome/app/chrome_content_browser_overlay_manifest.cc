// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_content_browser_overlay_manifest.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/common/available_offline_content.mojom.h"
#include "chrome/common/cache_stats_recorder.mojom.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "components/rappor/public/mojom/rappor_recorder.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"  // nogncheck
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
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
        .RequireCapability("ash", "system_ui")
        .RequireCapability("ash", "test")
        .RequireCapability("ash", "display")
        .RequireCapability("assistant", "assistant")
        .RequireCapability("assistant_audio_decoder", "assistant:audio_decoder")
        // Only used in the classic Ash case
        .RequireCapability("chrome", "input_device_controller")
        .RequireCapability("chrome_printing", "converter")
        .RequireCapability("cups_ipp_parser", "ipp_parser")
        .RequireCapability("device", "device:fingerprint")
        .RequireCapability("device", "device:geolocation_config")
        .RequireCapability("device", "device:geolocation_control")
        .RequireCapability("device", "device:ip_geolocator")
        .RequireCapability("ime", "input_engine")
        .RequireCapability("mirroring", "mirroring")
        .RequireCapability("nacl_broker", "browser")
        .RequireCapability("nacl_loader", "browser")
        .RequireCapability("noop", "noop")
        .RequireCapability("patch", "patch_file")
        .RequireCapability("profile_import", "import")
        .RequireCapability("removable_storage_writer",
                           "removable_storage_writer")
        .RequireCapability("secure_channel", "secure_channel")
        .RequireCapability("ui", "ime_registrar")
        .RequireCapability("ui", "input_device_controller")
        .RequireCapability("ui", "window_manager")
        .RequireCapability("unzip", "unzip_file")
        .RequireCapability("util_win", "util_win")
        .RequireCapability("xr_device_service", "xr_device_provider")
        .RequireCapability("xr_device_service", "xr_device_test_hook")
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "renderer",
            service_manager::Manifest::InterfaceList<
#if defined(OS_CHROMEOS)
                chromeos::cellular_setup::mojom::CellularSetup,
                chromeos::crostini_installer::mojom::PageHandlerFactory,
                chromeos::crostini_upgrader::mojom::PageHandlerFactory,
                chromeos::multidevice_setup::mojom::MultiDeviceSetup,
                chromeos::multidevice_setup::mojom::PrivilegedHostDeviceSetter,
                chromeos::network_config::mojom::CrosNetworkConfig,
#endif
                // WebUI-only interfaces go below this line. These should be
                // brokered through a dedicated interface, but they're here
                // for for now.
                feed_internals::mojom::PageHandler,
                new_tab_page::mojom::PageHandlerFactory,
#if !defined(OS_ANDROID)
                app_management::mojom::PageHandlerFactory,
#endif
                mojom::OmniboxPageHandler, mojom::ResetPasswordHandler,
                snippets_internals::mojom::PageHandlerFactory>())
        .Build()
  };
  return *manifest;
}
