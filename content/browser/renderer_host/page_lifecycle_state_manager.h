// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

#include "content/browser/renderer_host/input/one_shot_timeout_monitor.h"
#include "content/common/content_export.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {

class RenderViewHostImpl;

// A class responsible for managing the main lifecycle state of the blink::Page
// and communicating in to the RenderView. 1:1 with RenderViewHostImpl.
class CONTENT_EXPORT PageLifecycleStateManager {
 public:
  explicit PageLifecycleStateManager(
      RenderViewHostImpl* render_view_host_impl,
      blink::mojom::PageVisibilityState web_contents_visibility_state);
  ~PageLifecycleStateManager();

  void SetIsFrozen(bool frozen);
  void SetWebContentsVisibility(
      blink::mojom::PageVisibilityState visibility_state);
  void SetIsInBackForwardCache(
      bool is_in_back_forward_cache,
      base::Optional<base::TimeTicks> navigation_start);

 private:
  // Send mojo message to renderer if the effective (page) lifecycle state has
  // changed.
  void SendUpdatesToRendererIfNeeded(
      base::Optional<base::TimeTicks> navigation_start);

  // Calculates the per-page lifecycle state based on the per-tab / web contents
  // lifecycle state saved in this instance.
  blink::mojom::PageLifecycleStatePtr CalculatePageLifecycleState();

  void OnPageLifecycleChangedAck(
      blink::mojom::PageLifecycleStatePtr acknowledged_state);
  void OnBackForwardCacheTimeout();

  // This represents the frozen state set by |SetIsFrozen|, which corresponds to
  // WebContents::SetPageFrozen.  Effective frozen state, i.e. per-page frozen
  // state is computed based on |is_in_back_forward_cache_| and
  // |is_set_frozen_called_|.
  bool is_set_frozen_called_ = false;

  bool is_in_back_forward_cache_ = false;

  // This represents the visibility set by |SetVisibility|, which is web
  // contents visibility state. Effective visibility, i.e. per-page visibility
  // is computed based on |is_in_back_forward_cache_| and
  // |web_contents_visibility_|.
  blink::mojom::PageVisibilityState web_contents_visibility_;

  RenderViewHostImpl* render_view_host_impl_;

  // This is the per-page state computed based on web contents / tab lifecycle
  // states, i.e. |is_set_frozen_called_|, |is_in_back_forward_cache_| and
  // |web_contents_visibility_|.
  blink::mojom::PageLifecycleStatePtr last_acknowledged_state_;

  // This is the per-page state that is sent to renderer most lately.
  blink::mojom::PageLifecycleStatePtr last_state_sent_to_renderer_;

  std::unique_ptr<OneShotTimeoutMonitor> back_forward_cache_timeout_monitor_;
  // NOTE: This must be the last member.
  base::WeakPtrFactory<PageLifecycleStateManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
