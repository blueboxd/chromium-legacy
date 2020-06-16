// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_lifecycle_state_manager.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {
constexpr base::TimeDelta kBackForwardCacheTimeoutInSeconds =
    base::TimeDelta::FromSeconds(3);
}

namespace content {

PageLifecycleStateManager::PageLifecycleStateManager(
    RenderViewHostImpl* render_view_host_impl,
    blink::mojom::PageVisibilityState web_contents_visibility_state)
    : web_contents_visibility_(web_contents_visibility_state),
      render_view_host_impl_(render_view_host_impl) {
  last_acknowledged_state_ = CalculatePageLifecycleState();
  last_state_sent_to_renderer_ = last_acknowledged_state_.Clone();
}

PageLifecycleStateManager::~PageLifecycleStateManager() = default;

void PageLifecycleStateManager::SetIsFrozen(bool frozen) {
  if (is_set_frozen_called_ == frozen)
    return;
  is_set_frozen_called_ = frozen;

  SendUpdatesToRendererIfNeeded(base::nullopt);
}

void PageLifecycleStateManager::SetWebContentsVisibility(
    blink::mojom::PageVisibilityState visibility) {
  if (web_contents_visibility_ == visibility)
    return;

  web_contents_visibility_ = visibility;
  SendUpdatesToRendererIfNeeded(base::nullopt);
  // TODO(yuzus): When a page is frozen and made visible, the page should
  // automatically resume.
}

void PageLifecycleStateManager::SetIsInBackForwardCache(
    bool is_in_back_forward_cache,
    base::Optional<base::TimeTicks> navigation_start) {
  if (is_in_back_forward_cache_ == is_in_back_forward_cache)
    return;
  is_in_back_forward_cache_ = is_in_back_forward_cache;
  if (is_in_back_forward_cache) {
    // When a page is put into BackForwardCache, the page can run a busy loop.
    // Set a timeout monitor to check that the transition finishes within the
    // time limit.
    back_forward_cache_timeout_monitor_ =
        std::make_unique<OneShotTimeoutMonitor>(
            base::BindOnce(
                &PageLifecycleStateManager::OnBackForwardCacheTimeout,
                weak_ptr_factory_.GetWeakPtr()),
            kBackForwardCacheTimeoutInSeconds);
  }
  SendUpdatesToRendererIfNeeded(navigation_start);
}

void PageLifecycleStateManager::SendUpdatesToRendererIfNeeded(
    base::Optional<base::TimeTicks> navigation_start) {
  if (!render_view_host_impl_->GetAssociatedPageBroadcast()) {
    // For some tests, |render_view_host_impl_| does not have the associated
    // page.
    return;
  }

  auto new_state = CalculatePageLifecycleState();
  if (last_state_sent_to_renderer_ &&
      last_state_sent_to_renderer_.Equals(new_state)) {
    // TODO(yuzus): Send updates to renderer only when the effective state (per
    // page lifecycle state) has changed since last sent to renderer. It is
    // possible that the web contents state has changed but the effective state
    // has not.
  }

  last_state_sent_to_renderer_ = new_state.Clone();
  auto state = new_state.Clone();
  render_view_host_impl_->GetAssociatedPageBroadcast()->SetPageLifecycleState(
      std::move(state), std::move(navigation_start),
      base::BindOnce(&PageLifecycleStateManager::OnPageLifecycleChangedAck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(new_state)));
}

blink::mojom::PageLifecycleStatePtr
PageLifecycleStateManager::CalculatePageLifecycleState() {
  auto state = blink::mojom::PageLifecycleState::New();
  state->is_in_back_forward_cache = is_in_back_forward_cache_;
  state->is_frozen = is_in_back_forward_cache_ ? true : is_set_frozen_called_;
  state->visibility = is_in_back_forward_cache_
                          ? blink::mojom::PageVisibilityState::kHidden
                          : web_contents_visibility_;
  return state;
}

void PageLifecycleStateManager::OnPageLifecycleChangedAck(
    blink::mojom::PageLifecycleStatePtr acknowledged_state) {
  last_acknowledged_state_ = std::move(acknowledged_state);

  if (last_acknowledged_state_->is_in_back_forward_cache) {
    back_forward_cache_timeout_monitor_.reset(nullptr);
  }
}

void PageLifecycleStateManager::OnBackForwardCacheTimeout() {
  DCHECK(!last_acknowledged_state_->is_in_back_forward_cache);
  render_view_host_impl_->OnBackForwardCacheTimeout();
  back_forward_cache_timeout_monitor_.reset(nullptr);
}

}  // namespace content
