// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_resource_writer_impl.h"

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

}  // namespace

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
