// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_handle_core.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/service_worker/service_worker_utils.h"

namespace content {

ServiceWorkerNavigationHandleCore::ServiceWorkerNavigationHandleCore(
    base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle,
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper), ui_handle_(ui_handle) {
  // The ServiceWorkerNavigationHandleCore is created on the UI thread but
  // should only be accessed from the core thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerNavigationHandleCore::~ServiceWorkerNavigationHandleCore() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void ServiceWorkerNavigationHandleCore::OnBeginNavigationCommit(
    int render_process_id,
    int render_frame_id,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (container_host_) {
    container_host_->OnBeginNavigationCommit(render_process_id, render_frame_id,
                                             cross_origin_embedder_policy);
  }
}

void ServiceWorkerNavigationHandleCore::OnBeginWorkerCommit(
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (container_host_) {
    container_host_->CompleteWebWorkerPreparation(cross_origin_embedder_policy);
  }
}

}  // namespace content
