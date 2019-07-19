// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_overlay_manifests.h"

#include "base/no_destructor.h"
#include "components/services/patch/public/cpp/manifest.h"
#include "components/services/patch/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/cpp/manifest.h"
#include "components/services/unzip/public/mojom/constants.mojom.h"
#include "services/identity/public/cpp/manifest.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetChromeWebBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .RequireCapability(identity::mojom::kServiceName, "identity_accessor")
          .RequireCapability(patch::mojom::kServiceName, "patch_file")
          .RequireCapability(unzip::mojom::kServiceName, "unzip_file")
          .PackageService(identity::GetManifest())
          .Build()};

  return *manifest;
}

const service_manager::Manifest& GetChromeWebPackagedServicesOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .PackageService(patch::GetManifest())
          .PackageService(unzip::GetManifest())
          .Build()};

  return *manifest;
}
