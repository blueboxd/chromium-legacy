// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/builtin_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/services/quarantine/public/cpp/manifest.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/manifest.h"
#include "chromeos/services/cellular_setup/public/cpp/manifest.h"
#include "chromeos/services/ime/public/cpp/manifest.h"
#include "chromeos/services/network_config/public/cpp/manifest.h"
#include "chromeos/services/secure_channel/public/cpp/manifest.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "chrome/services/wifi_util_win/public/cpp/manifest.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/utility/importer/profile_import_manifest.h"
#include "components/mirroring/service/manifest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/pdf_compositor/public/cpp/manifest.h"  // nogncheck
#endif

const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
      quarantine::GetQuarantineManifest(),
#if BUILDFLAG(ENABLE_PRINTING)
      printing::GetPdfCompositorManifest(),
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
#endif
  }};
  return *manifests;
}
