// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_

#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"

namespace content {

// The implementation of storage::mojom::ServiceWorkerResourceReader.
// Currently this class is an adaptor that uses ServiceWorkerResponseReader
// internally.
// TODO(crbug.com/1055677): Fork the implementation of
// ServiceWorkerResponseReader and stop using it.
class ServiceWorkerResourceReaderImpl
    : public storage::mojom::ServiceWorkerResourceReader {
 public:
  explicit ServiceWorkerResourceReaderImpl(
      std::unique_ptr<ServiceWorkerResponseReader> reader);

  ServiceWorkerResourceReaderImpl(const ServiceWorkerResourceReaderImpl&) =
      delete;
  ServiceWorkerResourceReaderImpl& operator=(
      const ServiceWorkerResourceReaderImpl&) = delete;

  ~ServiceWorkerResourceReaderImpl() override;

 private:
  // storage::mojom::ServiceWorkerResourceReader implementations:
  void ReadResponseHead(ReadResponseHeadCallback callback) override;
  void ReadData(int64_t size, ReadDataCallback callback) override;

  void DidReadDataComplete();

  const std::unique_ptr<ServiceWorkerResponseReader> reader_;

  class DataReader;
  std::unique_ptr<DataReader> data_reader_;

  base::WeakPtrFactory<ServiceWorkerResourceReaderImpl> weak_factory_{this};
};

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

// The implementation of storage::mojom::ServiceWorkerResourceMetadataWriter.
// Currently this class is an adaptor that uses
// ServiceWorkerResponseMetadataWriter internally.
// TODO(crbug.com/1055677): Fork the implementation of
// ServiceWorkerResponseMetadataWriter and stop using it.
class ServiceWorkerResourceMetadataWriterImpl
    : public storage::mojom::ServiceWorkerResourceMetadataWriter {
 public:
  explicit ServiceWorkerResourceMetadataWriterImpl(
      std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer);

  ServiceWorkerResourceMetadataWriterImpl(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;
  ServiceWorkerResourceMetadataWriterImpl& operator=(
      const ServiceWorkerResourceMetadataWriterImpl&) = delete;

  ~ServiceWorkerResourceMetadataWriterImpl() override;

 private:
  // storage::mojom::ServiceWorkerResourceMetadataWriter implementations:
  void WriteMetadata(mojo_base::BigBuffer data,
                     WriteMetadataCallback callback) override;

  const std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_RESOURCE_OPS_H_
