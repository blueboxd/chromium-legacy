// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/origin.h"

namespace content {

namespace {

void CreateOnIO(
    blink::mojom::ContentIndexServiceRequest request,
    const url::Origin& origin,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::MakeStrongBinding(std::make_unique<ContentIndexServiceImpl>(
                              origin, std::move(service_worker_context)),
                          std::move(request));
}

}  // namespace

// static
void ContentIndexServiceImpl::Create(
    blink::mojom::ContentIndexServiceRequest request,
    RenderProcessHost* render_process_host,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());
  auto service_worker_context =
      base::WrapRefCounted(storage_partition->GetServiceWorkerContext());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CreateOnIO, std::move(request), origin,
                     std::move(service_worker_context)));
}

ContentIndexServiceImpl::ContentIndexServiceImpl(
    const url::Origin& origin,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper)
    : content_index_database_(origin,
                              std::move(service_worker_context_wrapper)) {}

ContentIndexServiceImpl::~ContentIndexServiceImpl() = default;

void ContentIndexServiceImpl::Add(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const SkBitmap& icon,
    AddCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index_database_.AddEntry(service_worker_registration_id,
                                   std::move(description), icon,
                                   std::move(callback));
}

void ContentIndexServiceImpl::Delete(int64_t service_worker_registration_id,
                                     const std::string& content_id,
                                     DeleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index_database_.DeleteEntry(service_worker_registration_id,
                                      content_id, std::move(callback));
}

void ContentIndexServiceImpl::GetDescriptions(
    int64_t service_worker_registration_id,
    GetDescriptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index_database_.GetDescriptions(service_worker_registration_id,
                                          std::move(callback));
}

}  // namespace content
