// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/manifest.h"

#include <set>

#include "base/no_destructor.h"
#include "services/metrics/public/mojom/constants.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

namespace resource_coordinator {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Global Resource Coordinator")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("app",
                            service_manager::Manifest::InterfaceList<
                                memory_instrumentation::mojom::Coordinator>())
          .ExposeCapability(
              "heap_profiler_helper",
              service_manager::Manifest::InterfaceList<
                  memory_instrumentation::mojom::HeapProfilerHelper>())
          .ExposeCapability("tests", std::set<const char*>{"*"})
          .RequireCapability(metrics::mojom::kMetricsServiceName,
                             "url_keyed_metrics")
          .RequireCapability(service_manager::mojom::kServiceName,
                             "service_manager:service_manager")
          .Build()};
  return *manifest;
}

}  // namespace resource_coordinator
