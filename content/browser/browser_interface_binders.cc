// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_interface_binders.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_instance.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"

namespace content {
namespace internal {

void PopulateFrameBinders(RenderFrameHostImpl* rfhi,
                          service_manager::BinderMap* map) {
  map->Add<blink::mojom::AudioContextManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetAudioContextManager, base::Unretained(rfhi)));

  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileSystemManager, base::Unretained(rfhi)));
}

void PopulateBinderMapWithContext(
    RenderFrameHostImpl* rfhi,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map) {}

void PopulateBinderMap(RenderFrameHostImpl* rfhi,
                       service_manager::BinderMap* map) {
  PopulateFrameBinders(rfhi, map);
}

RenderFrameHost* GetContextForHost(RenderFrameHostImpl* rfhi) {
  return rfhi;
}

void PopulateDedicatedWorkerBinders(DedicatedWorkerHost* dwh,
                                    service_manager::BinderMap* map) {}

const url::Origin& GetContextForHost(DedicatedWorkerHost* dwh) {
  return dwh->GetOrigin();
}

void PopulateBinderMapWithContext(
    DedicatedWorkerHost* dwh,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  RenderProcessHostImpl* rphi =
      static_cast<RenderProcessHostImpl*>(dwh->GetProcessHost());
  // TODO(https://crbug.com/873661): Pass origin to FileSystemManager.
  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderProcessHostImpl::BindFileSystemManager, base::Unretained(rphi)));
}

void PopulateBinderMap(DedicatedWorkerHost* dwh,
                       service_manager::BinderMap* map) {
  PopulateDedicatedWorkerBinders(dwh, map);
}

url::Origin GetContextForHost(SharedWorkerHost* swh) {
  return url::Origin::Create(swh->instance()->url());
}

void PopulateSharedWorkerBinders(SharedWorkerHost* swh,
                                 service_manager::BinderMap* map) {}

void PopulateBinderMapWithContext(
    SharedWorkerHost* swh,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  RenderProcessHostImpl* rphi =
      static_cast<RenderProcessHostImpl*>(swh->GetProcessHost());
  // TODO(https://crbug.com/873661): Pass origin to FileSystemManager.
  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderProcessHostImpl::BindFileSystemManager, base::Unretained(rphi)));
}

void PopulateBinderMap(SharedWorkerHost* swh, service_manager::BinderMap* map) {
  PopulateSharedWorkerBinders(swh, map);
}

}  // namespace internal
}  // namespace content