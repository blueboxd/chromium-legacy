// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_CHROMEOS)
#include "chrome/services/file_util/public/mojom/zip_file_creator.mojom.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#endif

const service_manager::Manifest& GetFileUtilManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(chrome::mojom::kFileUtilServiceName)
        .WithDisplayName(IDS_UTILITY_PROCESS_FILE_UTILITY_NAME)
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
                .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin)
                .WithSandboxType("utility")
                .WithInstanceSharingPolicy(
                    service_manager::Manifest::InstanceSharingPolicy::
                        kSharedAcrossGroups)
                .Build())
#if defined(OS_CHROMEOS)
        .ExposeCapability("zip_file", service_manager::Manifest::InterfaceList<
                                          chrome::mojom::ZipFileCreator>())
#endif
#if BUILDFLAG(FULL_SAFE_BROWSING)
        .ExposeCapability("analyze_archive",
                          service_manager::Manifest::InterfaceList<
                              chrome::mojom::SafeArchiveAnalyzer>())
#endif
        .Build()
  };
  return *manifest;
}
