// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace resource_coordinator {

ResourceCoordinatorService::ResourceCoordinatorService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::nullopt /* idle_timeout */) {}

ResourceCoordinatorService::~ResourceCoordinatorService() = default;

void ResourceCoordinatorService::OnStart() {
  // TODO(chiniforooshan): The abstract class Coordinator in the
  // public/cpp/memory_instrumentation directory should not be needed anymore.
  // We should be able to delete that and rename
  // memory_instrumentation::CoordinatorImpl to
  // memory_instrumentation::Coordinator.
  memory_instrumentation_coordinator_ =
      std::make_unique<memory_instrumentation::CoordinatorImpl>(
          service_binding_.GetConnector());
  registry_.AddInterface(base::BindRepeating(
      &memory_instrumentation::CoordinatorImpl::BindCoordinatorRequest,
      base::Unretained(memory_instrumentation_coordinator_.get())));
  registry_.AddInterface(base::BindRepeating(
      &memory_instrumentation::CoordinatorImpl::BindHeapProfilerHelperRequest,
      base::Unretained(memory_instrumentation_coordinator_.get())));
}

void ResourceCoordinatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

}  // namespace resource_coordinator
