// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_resource_ops.h"

#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/net_adapters.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

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
    std::move(callback).Run(status, /*response_head=*/nullptr,
                            /*metadata=*/base::nullopt);
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

  base::Optional<mojo_base::BigBuffer> metadata;
  if (http_info->metadata) {
    metadata = mojo_base::BigBuffer(base::as_bytes(base::make_span(
        http_info->metadata->data(), http_info->metadata->size())));
  }

  std::move(callback).Run(status, std::move(head), std::move(metadata));
}

}  // namespace

class ServiceWorkerResourceReaderImpl::DataReader {
 public:
  DataReader(base::WeakPtr<ServiceWorkerResourceReaderImpl> owner,
             size_t total_bytes_to_read,
             mojo::ScopedDataPipeProducerHandle producer_handle)
      : owner_(std::move(owner)),
        total_bytes_to_read_(total_bytes_to_read),
        producer_handle_(std::move(producer_handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 base::SequencedTaskRunnerHandle::Get()) {
    watcher_.Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::BindRepeating(&DataReader::OnWritable,
                                       weak_factory_.GetWeakPtr()));
    watcher_.ArmOrNotify();
  }
  ~DataReader() = default;

  DataReader(const DataReader&) = delete;
  DataReader operator=(const DataReader&) = delete;

 private:
  void OnWritable(MojoResult) {
    DCHECK(producer_handle_.is_valid());
    DCHECK(!pending_buffer_);

    if (!owner_) {
      Complete(net::ERR_ABORTED);
      return;
    }

    uint32_t num_bytes = 0;
    MojoResult rv = network::NetToMojoPendingBuffer::BeginWrite(
        &producer_handle_, &pending_buffer_, &num_bytes);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_BUSY:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        Complete(net::ERR_ABORTED);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        watcher_.ArmOrNotify();
        return;
      case MOJO_RESULT_OK:
        // |producer__handle_| must have been taken by |pending_buffer_|.
        DCHECK(pending_buffer_);
        DCHECK(!producer_handle_.is_valid());
        break;
    }

    num_bytes = std::min(num_bytes, blink::BlobUtils::GetDataPipeChunkSize());
    auto buffer =
        base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_buffer_.get());
    owner_->reader_->ReadData(
        buffer.get(), num_bytes,
        base::BindOnce(&DataReader::DidReadData, weak_factory_.GetWeakPtr()));
  }

  void DidReadData(int read_bytes) {
    if (read_bytes < 0) {
      Complete(read_bytes);
      return;
    }

    producer_handle_ = pending_buffer_->Complete(read_bytes);
    DCHECK(producer_handle_.is_valid());
    pending_buffer_.reset();
    current_bytes_read_ += read_bytes;

    if (read_bytes == 0 || current_bytes_read_ == total_bytes_to_read_) {
      // All data has been read.
      Complete(current_bytes_read_);
      return;
    }
    watcher_.ArmOrNotify();
  }

  void Complete(int status) {
    watcher_.Cancel();
    producer_handle_.reset();
    if (owner_) {
      owner_->DidReadDataComplete();
    }
  }

  base::WeakPtr<ServiceWorkerResourceReaderImpl> owner_;
  const size_t total_bytes_to_read_;
  size_t current_bytes_read_ = 0;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::SimpleWatcher watcher_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;

  base::WeakPtrFactory<DataReader> weak_factory_{this};
};

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

void ServiceWorkerResourceReaderImpl::ReadData(int64_t size,
                                               ReadDataCallback callback) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(size);

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(mojo::ScopedDataPipeConsumerHandle());
    return;
  }

  data_reader_ = std::make_unique<DataReader>(weak_factory_.GetWeakPtr(), size,
                                              std::move(producer_handle));
  std::move(callback).Run(std::move(consumer_handle));
}

void ServiceWorkerResourceReaderImpl::DidReadDataComplete() {
  DCHECK(data_reader_);
  data_reader_.reset();
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

ServiceWorkerResourceMetadataWriterImpl::
    ServiceWorkerResourceMetadataWriterImpl(
        std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ServiceWorkerResourceMetadataWriterImpl::
    ~ServiceWorkerResourceMetadataWriterImpl() = default;

void ServiceWorkerResourceMetadataWriterImpl::WriteMetadata(
    mojo_base::BigBuffer data,
    WriteMetadataCallback callback) {
  int buf_len = data.size();
  auto buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  writer_->WriteMetadata(buffer.get(), buf_len, std::move(callback));
}

}  // namespace content
