// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/filesystem/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/filesystem/public/mojom/file_system.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace filesystem {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName("filesystem")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithSandboxType("none")
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kStandaloneExecutable)
                  .Build())
          .ExposeCapability(
              "filesystem:filesystem",
              service_manager::Manifest::InterfaceList<mojom::FileSystem>())
          .Build()};
  return *manifest;
}

}  // namespace filesystem
