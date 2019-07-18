// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/font/public/mojom/constants.mojom.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace font_service {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Font Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "font_service",
              service_manager::Manifest::InterfaceList<mojom::FontService>())
          .Build()};
  return *manifest;
}

}  // namespace font_service
