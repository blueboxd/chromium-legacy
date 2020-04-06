// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_resource_ops.h"

#include "content/browser/service_worker/service_worker_loader_helpers.h"

namespace content {

namespace {

// TODO(bashi): Don't duplicate. This is the same as the BigIObuffer defined in
// //content/browser/code_cache/generated_code_cache.cc
class BigIOBuffer : public net::IOBufferWithSize {
 public:
  explicit BigIOBuffer(mojo_base::BigBuffer buffer);

  BigIOBuffer(const BigIOBuffer&) = delete;
  BigIOBuffer& operator=(const BigIOBuffer&) = delete;

 protected:
  ~BigIOBuffer() override;

 private:
  mojo_base::BigBuffer buffer_;
};

BigIOBuffer::BigIOBuffer(mojo_base::BigBuffer buffer)
    : net::IOBufferWithSize(nullptr, buffer.size()),
      buffer_(std::move(buffer)) {
  data_ = reinterpret_cast<char*>(buffer_.data());
}

BigIOBuffer::~BigIOBuffer() {
  data_ = nullptr;
  size_ = 0UL;
}

void DidReadInfo(
    scoped_refptr<HttpResponseInfoIOBuffer> buffer,
    ServiceWorkerResourceReaderImpl::ReadResponseHeadCallback callback,
    int status) {
  DCHECK(buffer);
  const net::HttpResponseInfo* http_info = buffer->http_info.get();
  if (!http_info) {
    DCHECK_LT(status, 0);
    std::move(callback).Run(status, /*response_head=*/nullptr);
    return;
  }

  // URLResponseHead fields filled here are the same as
  // ServiceWorkerUtils::CreateResourceResponseHeadAndMetadata(). Once
  // https://crbug.com/1060076 is done CreateResourceResponseHeadAndMetadata()
  // will be removed, but we still need HttpResponseInfo -> URLResponseHead
  // conversion to restore a response from the storage.
  // TODO(bashi): Remove the above comment ater the issue is closed.
  auto head = network::mojom::URLResponseHead::New();
  head->request_time = http_info->request_time;
  head->response_time = http_info->response_time;
  head->headers = http_info->headers;
  head->headers->GetMimeType(&head->mime_type);
  head->headers->GetCharset(&head->charset);
  head->content_length = buffer->response_data_size;
  head->was_fetched_via_spdy = http_info->was_fetched_via_spdy;
  head->was_alpn_negotiated = http_info->was_alpn_negotiated;
  head->connection_info = http_info->connection_info;
  head->alpn_negotiated_protocol = http_info->alpn_negotiated_protocol;
  head->remote_endpoint = http_info->remote_endpoint;
  head->cert_status = http_info->ssl_info.cert_status;
  head->ssl_info = http_info->ssl_info;

  std::move(callback).Run(status, std::move(head));
}

}  // namespace

ServiceWorkerResourceReaderImpl::ServiceWorkerResourceReaderImpl(
    std::unique_ptr<ServiceWorkerResponseReader> reader)
    : reader_(std::move(reader)) {
  DCHECK(reader_);
}

ServiceWorkerResourceReaderImpl::~ServiceWorkerResourceReaderImpl() = default;

void ServiceWorkerResourceReaderImpl::ReadResponseHead(
    ReadResponseHeadCallback callback) {
  auto buffer = base::MakeRefCounted<HttpResponseInfoIOBuffer>();
  HttpResponseInfoIOBuffer* raw_buffer = buffer.get();
  reader_->ReadInfo(raw_buffer, base::BindOnce(&DidReadInfo, std::move(buffer),
                                               std::move(callback)));
}

ServiceWorkerResourceWriterImpl::ServiceWorkerResourceWriterImpl(
    std::unique_ptr<ServiceWorkerResponseWriter> writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ServiceWorkerResourceWriterImpl::~ServiceWorkerResourceWriterImpl() = default;

void ServiceWorkerResourceWriterImpl::WriteResponseHead(
    network::mojom::URLResponseHeadPtr response_head,
    WriteResponseHeadCallback callback) {
  blink::ServiceWorkerStatusCode service_worker_status;
  network::URLLoaderCompletionStatus completion_status;
  std::string error_message;
  std::unique_ptr<net::HttpResponseInfo> response_info =
      service_worker_loader_helpers::CreateHttpResponseInfoAndCheckHeaders(
          *response_head, &service_worker_status, &completion_status,
          &error_message);
  if (!response_info) {
    DCHECK_NE(net::OK, completion_status.error_code);
    std::move(callback).Run(completion_status.error_code);
    return;
  }

  auto info_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(std::move(response_info));
  writer_->WriteInfo(info_buffer.get(), std::move(callback));
}

void ServiceWorkerResourceWriterImpl::WriteData(mojo_base::BigBuffer data,
                                                WriteDataCallback callback) {
  int buf_len = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  writer_->WriteData(buffer.get(), buf_len, std::move(callback));
}

}  // namespace content
