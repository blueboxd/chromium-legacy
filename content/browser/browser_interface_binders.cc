// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_interface_binders.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"

namespace content {
namespace internal {

void PopulateFrameBinders(RenderFrameHostImpl* rfhi,
                          service_manager::BinderMap* map) {
  map->Add<blink::mojom::AudioContextManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetAudioContextManager, base::Unretained(rfhi)));
}

void PopulateBinderMap(RenderFrameHostImpl* rfhi,
                       service_manager::BinderMap* map) {
  PopulateFrameBinders(rfhi, map);
}

void PopulateDedicatedWorkerBinders(DedicatedWorkerHost* dwh,
                                    service_manager::BinderMap* map) {}

void PopulateBinderMap(DedicatedWorkerHost* dwh,
                       service_manager::BinderMap* map) {
  PopulateDedicatedWorkerBinders(dwh, map);
}

}  // namespace internal
}  // namespace content