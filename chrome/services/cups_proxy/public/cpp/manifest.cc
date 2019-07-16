// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/services/cups_proxy/public/mojom/constants.mojom.h"
#include "chrome/services/cups_proxy/public/mojom/proxy.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace printing {

const service_manager::Manifest& GetCupsProxyManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kCupsProxyServiceName)
          .WithDisplayName("CupsProxyService")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kInProcessBuiltin)
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSingleton)
                  .Build())
          .ExposeCapability(mojom::kStartCupsProxyServiceCapability,
                            service_manager::Manifest::InterfaceList<
                                mojom::StartCupsProxyService>())
          .Build()};
  return *manifest;
}

}  // namespace printing
}  // namespace chromeos
