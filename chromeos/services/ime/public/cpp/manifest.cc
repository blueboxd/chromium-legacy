// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace ime {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName(IDS_IME_SERVICE_DISPLAY_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability(
              "input_engine",
              service_manager::Manifest::InterfaceList<
                  mojom::InputEngineManager, mojom::PlatformAccessClient>())
          .Build()};
  return *manifest;
}

}  // namespace ime
}  // namespace chromeos
