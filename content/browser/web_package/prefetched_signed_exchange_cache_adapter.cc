// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/prefetched_signed_exchange_cache_adapter.h"

#include "content/browser/loader/prefetch_url_loader.h"
#include "storage/browser/blob/blob_builder_from_stream.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {

PrefetchedSignedExchangeCacheAdapter::PrefetchedSignedExchangeCacheAdapter(
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const GURL& request_url,
    PrefetchURLLoader* prefetch_url_loader)
    : prefetched_signed_exchange_cache_(
          std::move(prefetched_signed_exchange_cache)),
      blob_storage_context_(std::move(blob_storage_context)),
      cached_exchange_(
          std::make_unique<PrefetchedSignedExchangeCache::Entry>()),
      prefetch_url_loader_(prefetch_url_loader) {
  cached_exchange_->SetOuterUrl(request_url);
}

PrefetchedSignedExchangeCacheAdapter::~PrefetchedSignedExchangeCacheAdapter() {
  if (blob_builder_from_stream_)
    blob_builder_from_stream_->Abort();
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveOuterResponse(
    const network::ResourceResponseHead& response) {
  cached_exchange_->SetOuterResponse(
      std::make_unique<network::ResourceResponseHead>(response));
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveRedirect(
    const GURL& new_url,
    const base::Optional<net::SHA256HashValue> header_integrity,
    const base::Time& signature_expire_time) {
  DCHECK(header_integrity);
  DCHECK(!signature_expire_time.is_null());
  cached_exchange_->SetHeaderIntegrity(
      std::make_unique<net::SHA256HashValue>(*header_integrity));
  cached_exchange_->SetInnerUrl(new_url);
  cached_exchange_->SetSignatureExpireTime(signature_expire_time);
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveInnerResponse(
    const network::ResourceResponseHead& response) {
  std::unique_ptr<network::ResourceResponseHead> inner_response =
      std::make_unique<network::ResourceResponseHead>(response);
  inner_response->was_in_prefetch_cache = true;
  cached_exchange_->SetInnerResponse(std::move(inner_response));
}

void PrefetchedSignedExchangeCacheAdapter::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(cached_exchange_->inner_response());
  DCHECK(!cached_exchange_->completion_status());
  DCHECK(blob_storage_context_);
  uint64_t length_hint = 0;
  if (cached_exchange_->inner_response()->content_length > 0) {
    length_hint = cached_exchange_->inner_response()->content_length;
  }
  blob_builder_from_stream_ = std::make_unique<storage::BlobBuilderFromStream>(
      blob_storage_context_, "" /* content_type */,
      "" /* content_disposition */,
      base::BindOnce(&PrefetchedSignedExchangeCacheAdapter::StreamingBlobDone,
                     base::Unretained(this)));
  blob_builder_from_stream_->Start(length_hint, std::move(body),
                                   nullptr /*  progress_client */);
}

void PrefetchedSignedExchangeCacheAdapter::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  cached_exchange_->SetCompletionStatus(
      std::make_unique<network::URLLoaderCompletionStatus>(status));
  MaybeCallOnSignedExchangeStored();
}

void PrefetchedSignedExchangeCacheAdapter::StreamingBlobDone(
    storage::BlobBuilderFromStream* builder,
    std::unique_ptr<storage::BlobDataHandle> result) {
  blob_builder_from_stream_.reset();
  cached_exchange_->SetBlobDataHandle(std::move(result));
  MaybeCallOnSignedExchangeStored();
}

void PrefetchedSignedExchangeCacheAdapter::MaybeCallOnSignedExchangeStored() {
  if (!cached_exchange_->completion_status() || blob_builder_from_stream_) {
    return;
  }

  const network::URLLoaderCompletionStatus completion_status =
      *cached_exchange_->completion_status();

  // When SignedExchangePrefetchHandler failed to load the response (eg: invalid
  // signed exchange format), the inner response is not set. In that case, we
  // don't send the body to avoid the DCHECK() failure in URLLoaderClientImpl::
  // OnStartLoadingResponseBody().
  const bool should_send_body = cached_exchange_->inner_response().get();

  if (completion_status.error_code == net::OK &&
      cached_exchange_->blob_data_handle() &&
      cached_exchange_->blob_data_handle()->size()) {
    prefetched_signed_exchange_cache_->Store(std::move(cached_exchange_));
  }

  if (should_send_body) {
    if (!prefetch_url_loader_->SendEmptyBody())
      return;
  }
  prefetch_url_loader_->SendOnComplete(completion_status);
}

}  // namespace content
