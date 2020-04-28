// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : owner_(owner), context_(context) {}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64_t ServiceWorkerScriptCacheMap::LookupResourceId(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return blink::mojom::kInvalidServiceWorkerResourceId;
  return found->second->resource_id;
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(const GURL& url,
                                                       int64_t resource_id) {
  DCHECK_EQ(blink::mojom::kInvalidServiceWorkerResourceId,
            LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING)
      << owner_->status();
  if (!context_)
    return;  // Our storage has been wiped via DeleteAndStartOver.
  resource_map_[url] =
      storage::mojom::ServiceWorkerResourceRecord::New(resource_id, url, -1);
  context_->registry()->StoreUncommittedResourceId(resource_id,
                                                   owner_->scope().GetOrigin());
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url,
    int64_t size_bytes,
    net::Error net_error,
    const std::string& status_message) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerResourceId,
            LookupResourceId(url));
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING ||
         owner_->status() == ServiceWorkerVersion::REDUNDANT);
  if (!context_)
    return;  // Our storage has been wiped via DeleteAndStartOver.

  if (net_error != net::OK) {
    context_->registry()->DoomUncommittedResource(LookupResourceId(url));
    resource_map_.erase(url);
    if (owner_->script_url() == url) {
      main_script_net_error_ = net_error;
      main_script_status_message_ = status_message;
    }
  } else {
    // |size_bytes| should not be negative when caching finished successfully.
    CHECK_GE(size_bytes, 0);
    resource_map_[url]->size_bytes = size_bytes;
  }
}

void ServiceWorkerScriptCacheMap::GetResources(
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>* resources) {
  DCHECK(resources->empty());
  for (ResourceMap::const_iterator it = resource_map_.begin();
       it != resource_map_.end();
       ++it) {
    resources->push_back(it->second->Clone());
  }
}

void ServiceWorkerScriptCacheMap::SetResources(
    const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
        resources) {
  DCHECK(resource_map_.empty());
  for (auto it = resources.begin(); it != resources.end(); ++it) {
    resource_map_[(*it)->url] = (*it)->Clone();
  }
}

void ServiceWorkerScriptCacheMap::WriteMetadata(
    const GURL& url,
    base::span<const uint8_t> data,
    net::CompletionOnceCallback callback) {
  if (!context_) {
    std::move(callback).Run(net::ERR_ABORTED);
    return;
  }

  auto found = resource_map_.find(url);
  if (found == resource_map_.end() ||
      found->second->resource_id ==
          blink::mojom::kInvalidServiceWorkerResourceId) {
    std::move(callback).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(data.size());
  if (data.size())
    memmove(buffer->data(), &data[0], data.size());
  std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer;
  writer = context_->storage()->CreateResponseMetadataWriter(
      found->second->resource_id);
  ServiceWorkerResponseMetadataWriter* raw_writer = writer.get();
  raw_writer->WriteMetadata(
      buffer.get(), data.size(),
      base::BindOnce(&ServiceWorkerScriptCacheMap::OnMetadataWritten,
                     weak_factory_.GetWeakPtr(), std::move(writer),
                     std::move(callback)));
}

void ServiceWorkerScriptCacheMap::ClearMetadata(
    const GURL& url,
    net::CompletionOnceCallback callback) {
  WriteMetadata(url, std::vector<uint8_t>(), std::move(callback));
}

void ServiceWorkerScriptCacheMap::OnMetadataWritten(
    std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer,
    net::CompletionOnceCallback callback,
    int result) {
  std::move(callback).Run(result);
}

}  // namespace content
