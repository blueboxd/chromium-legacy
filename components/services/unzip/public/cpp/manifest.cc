// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/unzip/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace unzip {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kServiceName)
        .WithDisplayName(IDS_UNZIP_SERVICE_DISPLAY_NAME)
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
#if !defined(OS_IOS)
                .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin)
#endif  // !defined(OS_IOS)
                .WithSandboxType("utility")
                .WithInstanceSharingPolicy(
                    service_manager::Manifest::InstanceSharingPolicy::
                        kSharedAcrossGroups)
                .Build())
        .ExposeCapability(
            "unzip_file",
            service_manager::Manifest::InterfaceList<mojom::Unzipper>())
        .Build()
  };
  return *manifest;
}

}  // namespace unzip
