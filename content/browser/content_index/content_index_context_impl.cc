// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

ContentIndexContextImpl::ContentIndexContextImpl(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : content_index_database_(browser_context,
                              std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContextImpl::GetIcon(
    int64_t service_worker_registration_id,
    const std::string& description_id,
    base::OnceCallback<void(SkBitmap)> icon_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexDatabase::GetIcon,
                     content_index_database_.GetWeakPtrForIO(),
                     service_worker_registration_id, description_id,
                     std::move(icon_callback)));
}

void ContentIndexContextImpl::GetAllEntries(GetAllEntriesCallback callback) {
  GetAllEntriesCallback wrapped_callback = base::BindOnce(
      [](GetAllEntriesCallback callback, blink::mojom::ContentIndexError error,
         std::vector<ContentIndexEntry> entries) {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(std::move(callback), error, std::move(entries)));
      },
      std::move(callback));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexDatabase::GetAllEntries,
                     content_index_database_.GetWeakPtrForIO(),
                     std::move(wrapped_callback)));
}

void ContentIndexContextImpl::GetEntry(int64_t service_worker_registration_id,
                                       const std::string& description_id,
                                       GetEntryCallback callback) {
  GetEntryCallback wrapped_callback = base::BindOnce(
      [](GetEntryCallback callback, base::Optional<ContentIndexEntry> entry) {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(std::move(callback), std::move(entry)));
      },
      std::move(callback));

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexDatabase::GetEntry,
                     content_index_database_.GetWeakPtrForIO(),
                     service_worker_registration_id, description_id,
                     std::move(wrapped_callback)));
}

void ContentIndexContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content_index_database_.Shutdown();
}

ContentIndexDatabase& ContentIndexContextImpl::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return content_index_database_;
}

ContentIndexContextImpl::~ContentIndexContextImpl() = default;

}  // namespace content
