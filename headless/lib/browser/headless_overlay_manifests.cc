// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_overlay_manifests.h"

#include "base/no_destructor.h"
#include "components/services/pdf_compositor/public/cpp/manifest.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace headless {

const service_manager::Manifest& GetHeadlessContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .RequireCapability(printing::mojom::kServiceName, "compositor")
          .Build()};

  return *manifest;
}

const service_manager::Manifest&
GetHeadlessContentPackagedServicesOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .PackageService(printing::GetPdfCompositorManifest())
          .Build()};

  return *manifest;
}

}  // namespace headless
