// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_lifecycle_state_manager.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

PageLifecycleStateManager::PageLifecycleStateManager(
    RenderViewHostImpl* render_view_host_impl)
    : render_view_host_impl_(render_view_host_impl) {}

PageLifecycleStateManager::~PageLifecycleStateManager() = default;

void PageLifecycleStateManager::SetIsFrozen(bool frozen) {
  if (!render_view_host_impl_->GetAssociatedPageBroadcast()) {
    // For some tests, |render_view_host_impl_| does not have the associated
    // page.
    return;
  }
  auto state = blink::mojom::PageLifecycleState::New();
  state->is_frozen = frozen;

  render_view_host_impl_->GetAssociatedPageBroadcast()->SetPageLifecycleState(
      std::move(state), base::BindOnce(&PageLifecycleStateManager::OnFreezeAck,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void PageLifecycleStateManager::OnFreezeAck() {
  // TODO(yuzus): Implement OnPageFrozen and send changes to the observers.
}

}  // namespace content
