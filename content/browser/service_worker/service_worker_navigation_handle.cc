// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_handle.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

ServiceWorkerNavigationHandle::ServiceWorkerNavigationHandle(
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_ = new ServiceWorkerNavigationHandleCore(weak_factory_.GetWeakPtr(),
                                                context_wrapper);
}

ServiceWorkerNavigationHandle::~ServiceWorkerNavigationHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delete the ServiceWorkerNavigationHandleCore on the IO thread.
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, core_);
}

void ServiceWorkerNavigationHandle::OnCreatedProviderHost(
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(provider_info->host_ptr_info.is_valid() &&
         provider_info->client_request.is_pending());

  provider_info_ = std::move(provider_info);
}

void ServiceWorkerNavigationHandle::OnBeginNavigationCommit(
    int render_process_id,
    int render_frame_id,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr* out_provider_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We may have failed to pre-create the provider host.
  if (!provider_info_)
    return;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ServiceWorkerNavigationHandleCore::OnBeginNavigationCommit,
          base::Unretained(core_), render_process_id, render_frame_id));
  *out_provider_info = std::move(provider_info_);
}

void ServiceWorkerNavigationHandle::OnBeginWorkerCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ServiceWorkerNavigationHandleCore::OnBeginWorkerCommit,
                     base::Unretained(core_)));
}

}  // namespace content
