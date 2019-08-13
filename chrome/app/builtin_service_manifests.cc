// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/builtin_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/manifest.h"
#include "chromeos/services/network_config/public/cpp/manifest.h"
#include "chromeos/services/secure_channel/public/cpp/manifest.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#endif

#if !defined(OS_ANDROID)
#include "components/mirroring/service/manifest.h"  // nogncheck
#endif

const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
#if !defined(OS_ANDROID)
      mirroring::GetManifest(),
#endif
#if defined(OS_CHROMEOS)
      ash::GetManifest(),
      chromeos::network_config::GetManifest(),
      chromeos::secure_channel::GetManifest(),
#endif
  }};
  return *manifests;
}
