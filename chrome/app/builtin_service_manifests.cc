// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/builtin_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/constants.mojom.h"
#include "chrome/services/file_util/public/cpp/manifest.h"
#include "components/services/patch/public/cpp/manifest.h"
#include "components/services/quarantine/public/cpp/manifest.h"
#include "components/services/unzip/public/cpp/manifest.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/preferences/public/cpp/local_state_manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/manifest.h"
#include "chrome/services/cups_ipp_parser/public/cpp/manifest.h"  // nogncheck
#include "chromeos/services/cellular_setup/public/cpp/manifest.h"
#include "chromeos/services/ime/public/cpp/manifest.h"
#include "chromeos/services/network_config/public/cpp/manifest.h"
#include "chromeos/services/secure_channel/public/cpp/manifest.h"
#endif

#if defined(OS_MACOSX)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "chrome/services/wifi_util_win/public/cpp/manifest.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/utility/importer/profile_import_manifest.h"
#include "components/mirroring/service/manifest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/pdf_compositor/public/cpp/manifest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/services/printing/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "chrome/services/isolated_xr_device/manifest.h"
#endif

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_IN_PROCESS) || \
    BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
#include "services/content/simple_browser/public/cpp/manifest.h"  // nogncheck
#endif

namespace {

const service_manager::Manifest& GetChromeManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(chrome::mojom::kServiceName)
        .WithDisplayName("Chrome")
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
                .WithExecutionMode(
                    service_manager::Manifest::ExecutionMode::kInProcessBuiltin)
                .WithInstanceSharingPolicy(
                    service_manager::Manifest::InstanceSharingPolicy::
                        kSharedAcrossGroups)
                .CanConnectToInstancesWithAnyId(true)
                .CanRegisterOtherServiceInstances(true)
                .Build())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_MACOSX)
                              spellcheck::mojom::SpellCheckPanelHost,
#endif
                              spellcheck::mojom::SpellCheckHost,
                              startup_metric_utils::mojom::StartupMetricHost>())
        .RequireCapability(chrome::mojom::kRendererServiceName, "browser")
        .Build()
  };
  return *manifest;
}

}  // namespace

const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
      GetChromeManifest(),
      GetFileUtilManifest(),
      patch::GetManifest(),
      unzip::GetManifest(),
      prefs::GetLocalStateManifest(),
      quarantine::GetQuarantineManifest(),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      GetRemovableStorageWriterManifest(),
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
      GetMediaGalleryUtilManifest(),
#endif
#if BUILDFLAG(ENABLE_PRINTING)
      printing::GetPdfCompositorManifest(),
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
      GetChromePrintingManifest(),
#endif
#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
      GetXrDeviceServiceManifest(),
#endif
#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_IN_PROCESS)
      simple_browser::GetManifest(),
#endif
#if defined(OS_WIN)
      GetWifiUtilWinManifest(),
#endif
#if !defined(OS_ANDROID)
      mirroring::GetManifest(),
      GetProfileImportManifest(),
#endif
#if defined(OS_CHROMEOS)
      ash::GetManifest(),
      chromeos::cellular_setup::GetManifest(),
      chromeos::ime::GetManifest(),
      chromeos::network_config::GetManifest(),
      chromeos::secure_channel::GetManifest(),
      GetCupsIppParserManifest(),
#endif
  }};
  return *manifests;
}
