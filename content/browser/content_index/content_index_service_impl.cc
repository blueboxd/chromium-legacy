// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/bad_message.h"
#include "content/browser/content_index/content_index_database.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

void CreateOnIO(blink::mojom::ContentIndexServiceRequest request,
                const url::Origin& origin,
                scoped_refptr<ContentIndexContextImpl> content_index_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::MakeStrongBinding(std::make_unique<ContentIndexServiceImpl>(
                              origin, std::move(content_index_context)),
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &CreateOnIO, std::move(request), origin,
          base::WrapRefCounted(storage_partition->GetContentIndexContext())));
}

ContentIndexServiceImpl::ContentIndexServiceImpl(
    const url::Origin& origin,
    scoped_refptr<ContentIndexContextImpl> content_index_context)
    : origin_(origin),
      content_index_context_(std::move(content_index_context)) {}

ContentIndexServiceImpl::~ContentIndexServiceImpl() = default;

void ContentIndexServiceImpl::Add(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const SkBitmap& icon,
    const GURL& launch_url,
    AddCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (icon.isNull() || icon.width() > kMaxIconDimension ||
      icon.height() > kMaxIconDimension) {
    mojo::ReportBadMessage("Invalid icon");
    std::move(callback).Run(blink::mojom::ContentIndexError::INVALID_PARAMETER);
    return;
  }

  if (!launch_url.is_valid() ||
      !origin_.IsSameOriginWith(url::Origin::Create(launch_url.GetOrigin()))) {
    mojo::ReportBadMessage("Invalid launch URL");
    std::move(callback).Run(blink::mojom::ContentIndexError::INVALID_PARAMETER);
    return;
  }

  content_index_context_->database().AddEntry(
      service_worker_registration_id, origin_, std::move(description), icon,
      launch_url, std::move(callback));
}

void ContentIndexServiceImpl::Delete(int64_t service_worker_registration_id,
                                     const std::string& content_id,
                                     DeleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index_context_->database().DeleteEntry(
      service_worker_registration_id, origin_, content_id, std::move(callback));
}

void ContentIndexServiceImpl::GetDescriptions(
    int64_t service_worker_registration_id,
    GetDescriptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index_context_->database().GetDescriptions(
      service_worker_registration_id, std::move(callback));
}

}  // namespace content
