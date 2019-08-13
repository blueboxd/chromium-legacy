// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/content_index/content_index_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace content {

ContentIndexContextImpl::ContentIndexContextImpl(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : provider_(browser_context->GetContentIndexProvider()),
      service_worker_context_(service_worker_context),
      content_index_database_(browser_context, service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContextImpl::GetIcons(int64_t service_worker_registration_id,
                                       const std::string& description_id,
                                       GetIconsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexDatabase::GetIcons,
                     content_index_database_.GetWeakPtrForIO(),
                     service_worker_registration_id, description_id,
                     std::move(callback)));
}

void ContentIndexContextImpl::GetAllEntries(GetAllEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetAllEntriesCallback wrapped_callback = base::BindOnce(
      [](GetAllEntriesCallback callback, blink::mojom::ContentIndexError error,
         std::vector<ContentIndexEntry> entries) {
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(std::move(callback), error, std::move(entries)));
      },
      std::move(callback));

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&ContentIndexDatabase::GetAllEntries,
                                content_index_database_.GetWeakPtrForIO(),
                                std::move(wrapped_callback)));
}

void ContentIndexContextImpl::GetEntry(int64_t service_worker_registration_id,
                                       const std::string& description_id,
                                       GetEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetEntryCallback wrapped_callback = base::BindOnce(
      [](GetEntryCallback callback, base::Optional<ContentIndexEntry> entry) {
        base::PostTask(FROM_HERE, {BrowserThread::UI},
                       base::BindOnce(std::move(callback), std::move(entry)));
      },
      std::move(callback));

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&ContentIndexDatabase::GetEntry,
                                content_index_database_.GetWeakPtrForIO(),
                                service_worker_registration_id, description_id,
                                std::move(wrapped_callback)));
}

void ContentIndexContextImpl::OnUserDeletedItem(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ContentIndexDatabase::DeleteEntry,
                     content_index_database_.GetWeakPtrForIO(),
                     service_worker_registration_id, origin, description_id,
                     base::BindOnce(&ContentIndexContextImpl::DidDeleteItem,
                                    this, service_worker_registration_id,
                                    origin, description_id)));
}

void ContentIndexContextImpl::DidDeleteItem(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id,
    blink::mojom::ContentIndexError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != blink::mojom::ContentIndexError::NONE)
    return;

  service_worker_context_->FindReadyRegistrationForId(
      service_worker_registration_id, origin.GetURL(),
      base::BindOnce(&ContentIndexContextImpl::StartActiveWorkerForDispatch,
                     this, description_id));
}

void ContentIndexContextImpl::StartActiveWorkerForDispatch(
    const std::string& description_id,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index::RecordDisptachStatus("Find", service_worker_status);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;

  ServiceWorkerVersion* service_worker_version = registration->active_version();
  DCHECK(service_worker_version);

  service_worker_version->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::CONTENT_DELETE,
      base::BindOnce(&ContentIndexContextImpl::DeliverMessageToWorker, this,
                     base::WrapRefCounted(service_worker_version),
                     std::move(registration), description_id));
}

void ContentIndexContextImpl::DeliverMessageToWorker(
    scoped_refptr<ServiceWorkerVersion> service_worker,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const std::string& description_id,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index::RecordDisptachStatus("Start", service_worker_status);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk)
    return;

  // Don't allow DB operations while the `contentdelete` event is firing.
  // This is to prevent re-registering the deleted content within the event.
  content_index_database_.BlockOrigin(service_worker->script_origin());

  int request_id = service_worker->StartRequest(
      ServiceWorkerMetrics::EventType::CONTENT_DELETE,
      base::BindOnce(&ContentIndexContextImpl::DidDispatchEvent, this,
                     service_worker->script_origin()));

  service_worker->endpoint()->DispatchContentDeleteEvent(
      description_id, service_worker->CreateSimpleEventCallback(request_id));
}

void ContentIndexContextImpl::DidDispatchEvent(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content_index::RecordDisptachStatus("Dispatch", service_worker_status);
  content_index_database_.UnblockOrigin(origin);
}

void ContentIndexContextImpl::GetIconSizes(
    blink::mojom::ContentCategory category,
    blink::mojom::ContentIndexService::GetIconSizesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<gfx::Size> icon_sizes;
  if (provider_)
    icon_sizes = provider_->GetIconSizes(category);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), std::move(icon_sizes)));
}

void ContentIndexContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  provider_ = nullptr;
  content_index_database_.Shutdown();
}

ContentIndexDatabase& ContentIndexContextImpl::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return content_index_database_;
}

ContentIndexContextImpl::~ContentIndexContextImpl() = default;

}  // namespace content
