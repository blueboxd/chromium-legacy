// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/pdf_compositor/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace printing {

const service_manager::Manifest& GetPdfCompositorManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName(IDS_PDF_COMPOSITOR_SERVICE_DISPLAY_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("pdf_compositor")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability(
              "compositor",
              service_manager::Manifest::InterfaceList<mojom::PdfCompositor>())
          .RequireCapability("content_system", "app")
          .RequireCapability("content_system", "sandbox_support")
          .RequireCapability("ui", "discardable_memory")
          .Build()};
  return *manifest;
}

}  // namespace printing
