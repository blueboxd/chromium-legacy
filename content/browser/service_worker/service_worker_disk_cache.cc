// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include <utility>

namespace content {

ServiceWorkerDiskCache::ServiceWorkerDiskCache()
    : AppCacheDiskCache(/*use_simple_cache=*/true) {}

ServiceWorkerResponseReader::ServiceWorkerResponseReader(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseReader(resource_id, std::move(disk_cache)) {}

ServiceWorkerResponseWriter::ServiceWorkerResponseWriter(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseWriter(resource_id, std::move(disk_cache)) {}

void ServiceWorkerResponseWriter::WriteResponseHead(
    const network::mojom::URLResponseHead& response_head,
    int response_data_size,
    net::CompletionOnceCallback callback) {
  // This is copied from CreateHttpResponseInfoAndCheckHeaders().
  // TODO(bashi): Use CreateHttpResponseInfoAndCheckHeaders()
  // once we remove URLResponseHead -> HttpResponseInfo -> URLResponseHead
  // conversion, which drops some information needed for validation
  // (e.g. mime type).
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  response_info->headers = response_head.headers;
  if (response_head.ssl_info.has_value())
    response_info->ssl_info = *response_head.ssl_info;
  response_info->was_fetched_via_spdy = response_head.was_fetched_via_spdy;
  response_info->was_alpn_negotiated = response_head.was_alpn_negotiated;
  response_info->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  response_info->connection_info = response_head.connection_info;
  response_info->remote_endpoint = response_head.remote_endpoint;
  response_info->response_time = response_head.response_time;

  auto info_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info));
  info_buffer->response_data_size = response_data_size;
  WriteInfo(info_buffer.get(), std::move(callback));
}

ServiceWorkerResponseMetadataWriter::ServiceWorkerResponseMetadataWriter(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseMetadataWriter(resource_id, std::move(disk_cache)) {}

}  // namespace content
