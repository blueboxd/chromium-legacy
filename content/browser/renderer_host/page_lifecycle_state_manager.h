// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {

class RenderViewHostImpl;

// A class responsible for managing the main lifecycle state of the blink::Page
// and communicating in to the RenderView. 1:1 with RenderViewHostImpl.
class CONTENT_EXPORT PageLifecycleStateManager {
 public:
  explicit PageLifecycleStateManager(RenderViewHostImpl* render_view_host_impl);
  ~PageLifecycleStateManager();

  void SetIsFrozen(bool frozen);

 private:
  void OnFreezeAck();

  RenderViewHostImpl* render_view_host_impl_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<PageLifecycleStateManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
