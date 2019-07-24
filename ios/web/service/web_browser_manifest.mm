// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/service/web_browser_manifest.h"

#include "base/no_destructor.h"
#include "ios/web/public/service/service_names.mojom.h"
#include "ios/web/public/web_client.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

const service_manager::Manifest& GetWebBrowserManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kBrowserServiceName)
          .WithDisplayName("Web")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .CanConnectToInstancesInAnyGroup(true)
                           .CanConnectToInstancesWithAnyId(true)
                           .CanRegisterOtherServiceInstances(true)
                           .Build())
          .RequireCapability(mojom::kBrowserServiceName, "")
          .RequireCapability(mojom::kSystemServiceName, "")
          .RequireCapability(service_manager::mojom::kServiceName,
                             "service_manager:service_manager")
          .Build()
          .Amend(GetWebClient()
                     ->GetServiceManifestOverlay(mojom::kBrowserServiceName)
                     .value_or(service_manager::Manifest()))};

  return *manifest;
}

}  // namespace web
