// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

#include <utility>

#include "content/common/service_worker/service_worker_utils.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

namespace {

void DidReadInfo(base::WeakPtr<AppCacheResponseIO> reader,
                 scoped_refptr<HttpResponseInfoIOBuffer> io_buffer,
                 ServiceWorkerResponseReader::ReadResponseHeadCallback callback,
                 int result) {
  DCHECK(io_buffer);
  if (result < 0) {
    std::move(callback).Run(result, /*response_head=*/nullptr,
                            /*metadata=*/nullptr);
    return;
  }

  DCHECK(io_buffer->http_info);
  auto response_and_metadata =
      ServiceWorkerUtils::CreateResourceResponseHeadAndMetadata(
          io_buffer->http_info.get(),
          /*options=*/network::mojom::kURLLoadOptionSendSSLInfoWithResponse,
          /*request_start_time=*/base::TimeTicks(),
          /*response_start_time=*/base::TimeTicks::Now(),
          io_buffer->response_data_size);
  std::move(callback).Run(result, std::move(response_and_metadata.head),
                          std::move(response_and_metadata.metadata));
}

}  // namespace

ServiceWorkerDiskCache::ServiceWorkerDiskCache()
    : AppCacheDiskCache(/*use_simple_cache=*/true) {}

ServiceWorkerResponseReader::ServiceWorkerResponseReader(
    int64_t resource_id,
    base::WeakPtr<AppCacheDiskCache> disk_cache)
    : AppCacheResponseReader(resource_id, std::move(disk_cache)) {}

void ServiceWorkerResponseReader::ReadResponseHead(
    ReadResponseHeadCallback callback) {
  auto io_buffer = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
  ReadInfo(io_buffer.get(), base::BindOnce(&DidReadInfo, GetWeakPtr(),
                                           io_buffer, std::move(callback)));
}

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
