// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_

#include "base/optional.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "mojo/public/cpp/system/data_pipe.h"

class GURL;

namespace base {
class Time;
}  // namespace base

namespace net {
struct SHA256HashValue;
}  // namespace net

namespace storage {
class BlobBuilderFromStream;
class BlobDataHandle;
}  // namespace storage

namespace content {
class PrefetchURLLoader;

// This class is used by PrefetchURLLoader to store the prefetched and verified
// signed exchanges to the PrefetchedSignedExchangeCache.
class PrefetchedSignedExchangeCacheAdapter {
 public:
  PrefetchedSignedExchangeCacheAdapter(
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      const GURL& request_url,
      PrefetchURLLoader* prefetch_url_loader);
  ~PrefetchedSignedExchangeCacheAdapter();

  void OnReceiveOuterResponse(const network::ResourceResponseHead& response);
  void OnReceiveRedirect(
      const GURL& new_url,
      const base::Optional<net::SHA256HashValue> header_integrity,
      const base::Time& signature_expire_time);
  void OnReceiveInnerResponse(const network::ResourceResponseHead& response);
  void OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

 private:
  void StreamingBlobDone(storage::BlobBuilderFromStream* builder,
                         std::unique_ptr<storage::BlobDataHandle> result);

  void MaybeCallOnSignedExchangeStored();

  // Holds the prefetched signed exchanges which will be used in the next
  // navigation. This is shared with RenderFrameHostImpl that created this.
  const scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Used to create a BlobDataHandle from a DataPipe of signed exchange's inner
  // response body to store to |prefetched_signed_exchange_cache_|.
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;

  // A temporary entry of PrefetchedSignedExchangeCache, which will be stored
  // to |prefetched_signed_exchange_cache_|.
  std::unique_ptr<PrefetchedSignedExchangeCache::Entry> cached_exchange_;

  // Used to create a BlobDataHandle from a DataPipe of signed exchange's inner
  // response body.
  std::unique_ptr<storage::BlobBuilderFromStream> blob_builder_from_stream_;

  // |prefetch_url_loader_| owns |this|.
  PrefetchURLLoader* prefetch_url_loader_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchedSignedExchangeCacheAdapter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_
