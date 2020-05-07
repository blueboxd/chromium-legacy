// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/variations_render_thread_observer.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/variations/net/omnibox_url_loader_throttle.h"
#include "components/variations/net/variations_url_loader_throttle.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace content {
namespace {

// VariationsRenderThreadObserver is mostly used on the main thread but
// GetVariationsHeader can be called from worker threads (e.g. for service
// workers) necessitating locking.
class VariationsData {
 public:
  void SetVariationsHeader(const std::string& variation_ids_header) {
    base::AutoLock lock(lock_);
    variations_header_ = variation_ids_header;
  }

  // Deliberately returns a copy.
  std::string GetVariationsHeader() const {
    base::AutoLock lock(lock_);
    return variations_header_;
  }

 private:
  mutable base::Lock lock_;
  std::string variations_header_ GUARDED_BY(lock_);
};

VariationsData* GetVariationsData() {
  static base::NoDestructor<VariationsData> variations_data;
  return variations_data.get();
}

}  // namespace

VariationsRenderThreadObserver::VariationsRenderThreadObserver() = default;

VariationsRenderThreadObserver::~VariationsRenderThreadObserver() = default;

// static
void VariationsRenderThreadObserver::AppendThrottleIfNeeded(
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  variations::OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(throttles);

  std::string variations_header = GetVariationsData()->GetVariationsHeader();
  if (!variations_header.empty()) {
    throttles->push_back(
        std::make_unique<variations::VariationsURLLoaderThrottle>(
            std::move(variations_header)));
  }
}

void VariationsRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::BindRepeating(
      &VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void VariationsRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      mojom::RendererVariationsConfiguration::Name_);
}

void VariationsRenderThreadObserver::SetVariationsHeader(
    const std::string& variation_ids_header) {
  GetVariationsData()->SetVariationsHeader(variation_ids_header);
}

void VariationsRenderThreadObserver::SetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->SetFieldTrialGroup(trial_name, group_name);
}

void VariationsRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::RendererVariationsConfiguration>
        receiver) {
  renderer_configuration_receiver_.reset();
  renderer_configuration_receiver_.Bind(std::move(receiver));
}

}  // namespace content
