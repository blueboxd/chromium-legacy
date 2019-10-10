// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

using performance_manager::mojom::InterventionPolicy;

}  // namespace

// static
std::unique_ptr<DocumentResourceCoordinator>
DocumentResourceCoordinator::MaybeCreate(
    service_manager::InterfaceProvider* interface_provider) {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled())
    return nullptr;

  return base::WrapUnique(new DocumentResourceCoordinator(interface_provider));
}

DocumentResourceCoordinator::DocumentResourceCoordinator(
    service_manager::InterfaceProvider* interface_provider) {
  interface_provider->GetInterface(service_.BindNewPipeAndPassReceiver());
  DCHECK(service_);
}

DocumentResourceCoordinator::~DocumentResourceCoordinator() = default;

void DocumentResourceCoordinator::SetNetworkAlmostIdle() {
  service_->SetNetworkAlmostIdle();
}

void DocumentResourceCoordinator::SetLifecycleState(
    performance_manager::mojom::LifecycleState state) {
  service_->SetLifecycleState(state);
}

void DocumentResourceCoordinator::SetHasNonEmptyBeforeUnload(
    bool has_nonempty_beforeunload) {
  service_->SetHasNonEmptyBeforeUnload(has_nonempty_beforeunload);
}

void DocumentResourceCoordinator::SetOriginTrialFreezePolicy(
    InterventionPolicy policy) {
  service_->SetOriginTrialFreezePolicy(policy);
}

void DocumentResourceCoordinator::SetIsAdFrame() {
  service_->SetIsAdFrame();
}

void DocumentResourceCoordinator::OnNonPersistentNotificationCreated() {
  service_->OnNonPersistentNotificationCreated();
}

}  // namespace blink
