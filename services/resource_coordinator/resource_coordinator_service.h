// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
#define SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"

#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace resource_coordinator {

class ResourceCoordinatorService : public service_manager::Service {
 public:
  explicit ResourceCoordinatorService(
      service_manager::mojom::ServiceRequest request);
  ~ResourceCoordinatorService() override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;
  std::unique_ptr<memory_instrumentation::CoordinatorImpl>
      memory_instrumentation_coordinator_;

  // WeakPtrFactory members should always come last so WeakPtrs are destructed
  // before other members.
  base::WeakPtrFactory<ResourceCoordinatorService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorService);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_SERVICE_H_
