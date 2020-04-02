// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_WRITER_IMPL_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_WRITER_IMPL_H_

#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"

namespace content {

// The implementation of storage::mojom::ServiceWorkerResourceWriter.
// Currently this class is an adaptor that uses ServiceWorkerResponseWriter
// internally.
// TODO(crbug.com/1055677): Fork the implementation of
// ServiceWorkerResponseWriter and stop using it.
class ServiceWorkerResourceWriterImpl
    : public storage::mojom::ServiceWorkerResourceWriter {
 public:
  explicit ServiceWorkerResourceWriterImpl(
      std::unique_ptr<ServiceWorkerResponseWriter> writer);

  ServiceWorkerResourceWriterImpl(const ServiceWorkerResourceWriterImpl&) =
      delete;
  ServiceWorkerResourceWriterImpl& operator=(
      const ServiceWorkerResourceWriterImpl&) = delete;

  ~ServiceWorkerResourceWriterImpl() override;

 private:
  // storage::mojom::ServiceWorkerResourceWriter implementations:
  void WriteResponseHead(network::mojom::URLResponseHeadPtr response_head,
                         WriteResponseHeadCallback callback) override;
  void WriteData(mojo_base::BigBuffer data,
                 WriteDataCallback callback) override;

  const std::unique_ptr<ServiceWorkerResponseWriter> writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_WRITER_IMPL_H_
