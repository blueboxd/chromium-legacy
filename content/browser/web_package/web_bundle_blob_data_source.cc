// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_blob_data_source.h"

#include "base/bit_cast.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/io_buffer.h"
#include "storage/browser/blob/blob_builder_from_stream.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/mojo_blob_reader.h"

namespace content {
namespace {

class MojoBlobReaderDelegate : public storage::MojoBlobReader::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(net::Error net_error)>;
  explicit MojoBlobReaderDelegate(CompletionCallback completion_callback)
      : completion_callback_(std::move(completion_callback)) {}
  ~MojoBlobReaderDelegate() override = default;
  RequestSideData DidCalculateSize(uint64_t total_size,
                                   uint64_t content_size) override {
    return DONT_REQUEST_SIDE_DATA;
  }
  void DidRead(int num_bytes) override {}
  void OnComplete(net::Error result, uint64_t total_written_bytes) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    std::move(completion_callback_).Run(result);
  }

 private:
  CompletionCallback completion_callback_;
  DISALLOW_COPY_AND_ASSIGN(MojoBlobReaderDelegate);
};

void OnReadComplete(
    data_decoder::mojom::BundleDataSource::ReadCallback callback,
    std::unique_ptr<storage::BlobReader> blob_reader,
    scoped_refptr<net::IOBufferWithSize> io_buf,
    int bytes_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (bytes_read != io_buf->size()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::vector<uint8_t> vec;
  vec.assign(bit_cast<uint8_t*>(io_buf->data()),
             bit_cast<uint8_t*>(io_buf->data()) + bytes_read);
  std::move(callback).Run(std::move(vec));
}

void OnCalculateSizeComplete(
    uint64_t offset,
    uint64_t length,
    data_decoder::mojom::BundleDataSource::ReadCallback callback,
    std::unique_ptr<storage::BlobReader> blob_reader,
    int net_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_error != net::OK) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto set_read_range_status = blob_reader->SetReadRange(offset, length);
  if (set_read_range_status != storage::BlobReader::Status::DONE) {
    DCHECK_EQ(set_read_range_status, storage::BlobReader::Status::NET_ERROR);
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto* raw_blob_reader = blob_reader.get();
  auto io_buf =
      base::MakeRefCounted<net::IOBufferWithSize>(static_cast<size_t>(length));
  auto on_read_callback = base::AdaptCallbackForRepeating(base::BindOnce(
      &OnReadComplete, std::move(callback), std::move(blob_reader), io_buf));
  int bytes_read;
  storage::BlobReader::Status read_status = raw_blob_reader->Read(
      io_buf.get(), io_buf->size(), &bytes_read, on_read_callback);
  if (read_status != storage::BlobReader::Status::IO_PENDING) {
    on_read_callback.Run(bytes_read);
  }
}

bool IsValidRange(uint64_t offset, uint64_t length, int64_t content_length) {
  int64_t offset_plus_length;
  if (!base::CheckAdd(offset, length).AssignIfValid(&offset_plus_length))
    return false;
  return offset_plus_length <= content_length;
}

}  // namespace

WebBundleBlobDataSource::WebBundleBlobDataSource(
    int64_t content_length,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(content_length, 0);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebBundleBlobDataSource::CreateCoreOnIO,
                     weak_factory_.GetWeakPtr(), content_length,
                     std::move(outer_response_body), std::move(endpoints),
                     std::move(blob_context_getter)));
}

WebBundleBlobDataSource::~WebBundleBlobDataSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (core_)
    base::DeleteSoon(FROM_HERE, {BrowserThread::IO}, std::move(core_));

  auto tasks = std::move(pending_get_core_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::AddReceiver(
    mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WaitForCore(base::BindOnce(&WebBundleBlobDataSource::AddReceiverImpl,
                             base::Unretained(this),
                             std::move(pending_receiver)));
}

void WebBundleBlobDataSource::AddReceiverImpl(
    mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
        pending_receiver) {
  if (!core_)
    return;
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&BlobDataSourceCore::AddReceiver, weak_core_,
                                std::move(pending_receiver)));
}

// static
void WebBundleBlobDataSource::CreateCoreOnIO(
    base::WeakPtr<WebBundleBlobDataSource> weak_ptr,
    int64_t content_length,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto core = std::make_unique<BlobDataSourceCore>(
      content_length, std::move(endpoints), std::move(blob_context_getter));
  core->Start(std::move(outer_response_body));
  auto weak_core = core->GetWeakPtr();
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&WebBundleBlobDataSource::SetCoreOnUI, std::move(weak_ptr),
                     std::move(weak_core), std::move(core)));
}

// static
void WebBundleBlobDataSource::SetCoreOnUI(
    base::WeakPtr<WebBundleBlobDataSource> weak_ptr,
    base::WeakPtr<BlobDataSourceCore> weak_core,
    std::unique_ptr<BlobDataSourceCore> core) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!weak_ptr) {
    // This happens when the WebBundleBlobDataSource was deleted before
    // SetCoreOnUI() is called.
    base::DeleteSoon(FROM_HERE, {BrowserThread::IO}, std::move(core));
    return;
  }
  weak_ptr->SetCoreOnUIImpl(std::move(weak_core), std::move(core));
}

void WebBundleBlobDataSource::ReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WaitForCore(base::BindOnce(&WebBundleBlobDataSource::ReadToDataPipeImpl,
                             base::Unretained(this), offset, length,
                             std::move(producer_handle), std::move(callback)));
}

void WebBundleBlobDataSource::WaitForCore(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (core_) {
    std::move(callback).Run();
    return;
  }
  pending_get_core_tasks_.push_back(std::move(callback));
}

void WebBundleBlobDataSource::SetCoreOnUIImpl(
    base::WeakPtr<BlobDataSourceCore> weak_core,
    std::unique_ptr<BlobDataSourceCore> core) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_core_ = std::move(weak_core);
  core_ = std::move(core);

  auto tasks = std::move(pending_get_core_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::ReadToDataPipeImpl(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!core_) {
    // This happens when |this| was deleted before SetCoreOnUI() is called.
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  CompletionCallback wrapped_callback = base::BindOnce(
      [](CompletionCallback callback, net::Error net_error) {
        DCHECK_CURRENTLY_ON(BrowserThread::IO);
        base::PostTask(FROM_HERE, {BrowserThread::UI},
                       base::BindOnce(std::move(callback), net_error));
      },
      std::move(callback));
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&BlobDataSourceCore::ReadToDataPipe, weak_core_,
                                offset, length, std::move(producer_handle),
                                std::move(wrapped_callback)));
}

WebBundleBlobDataSource::BlobDataSourceCore::BlobDataSourceCore(
    int64_t content_length,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    BrowserContext::BlobContextGetter blob_context_getter)
    : content_length_(content_length),
      endpoints_(std::move(endpoints)),
      blob_builder_from_stream_(std::make_unique<
                                storage::BlobBuilderFromStream>(
          std::move(blob_context_getter).Run(),
          "" /* content_type */,
          "" /* content_disposition */,
          base::BindOnce(
              &WebBundleBlobDataSource::BlobDataSourceCore::StreamingBlobDone,
              base::Unretained(this)))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(content_length_, 0);
}

WebBundleBlobDataSource::BlobDataSourceCore::~BlobDataSourceCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (blob_builder_from_stream_)
    std::move(blob_builder_from_stream_)->Abort();
}

void WebBundleBlobDataSource::BlobDataSourceCore::Start(
    mojo::ScopedDataPipeConsumerHandle outer_response_body) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_builder_from_stream_->Start(
      content_length_, std::move(outer_response_body),
      mojo::NullAssociatedRemote() /*  progress_client */);
}

void WebBundleBlobDataSource::BlobDataSourceCore::AddReceiver(
    mojo::PendingReceiver<data_decoder::mojom::BundleDataSource>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebBundleBlobDataSource::BlobDataSourceCore::ReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!IsValidRange(offset, length, content_length_)) {
    std::move(callback).Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  WaitForBlob(base::BindOnce(&WebBundleBlobDataSource::BlobDataSourceCore::
                                 OnBlobReadyForReadToDataPipe,
                             base::Unretained(this), offset, length,
                             std::move(producer_handle), std::move(callback)));
}

base::WeakPtr<WebBundleBlobDataSource::BlobDataSourceCore>
WebBundleBlobDataSource::BlobDataSourceCore::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return weak_factory_.GetWeakPtr();
}

void WebBundleBlobDataSource::BlobDataSourceCore::GetSize(
    GetSizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(content_length_);
}

void WebBundleBlobDataSource::BlobDataSourceCore::Read(uint64_t offset,
                                                       uint64_t length,
                                                       ReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!IsValidRange(offset, length, content_length_)) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  WaitForBlob(base::BindOnce(
      &WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForRead,
      base::Unretained(this), offset, length, std::move(callback)));
}

void WebBundleBlobDataSource::BlobDataSourceCore::StreamingBlobDone(
    storage::BlobBuilderFromStream* builder,
    std::unique_ptr<storage::BlobDataHandle> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Content length mismatch is treated as an error.
  if (result && (result->size() == base::checked_cast<size_t>(content_length_)))
    blob_ = std::move(result);
  blob_builder_from_stream_.reset();

  auto tasks = std::move(pending_get_blob_tasks_);
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

void WebBundleBlobDataSource::BlobDataSourceCore::WaitForBlob(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_builder_from_stream_) {
    std::move(callback).Run();
    return;
  }
  pending_get_blob_tasks_.push_back(std::move(callback));
}

void WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForRead(
    uint64_t offset,
    uint64_t length,
    ReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  auto blob_reader = blob_->CreateReader();
  auto* raw_blob_reader = blob_reader.get();
  auto on_calculate_complete = base::AdaptCallbackForRepeating(
      base::BindOnce(&OnCalculateSizeComplete, offset, length,
                     std::move(callback), std::move(blob_reader)));
  auto status = raw_blob_reader->CalculateSize(on_calculate_complete);
  if (status != storage::BlobReader::Status::IO_PENDING) {
    on_calculate_complete.Run(status == storage::BlobReader::Status::NET_ERROR
                                  ? raw_blob_reader->net_error()
                                  : net::OK);
  }
}

void WebBundleBlobDataSource::BlobDataSourceCore::OnBlobReadyForReadToDataPipe(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob_) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  storage::MojoBlobReader::Create(
      blob_.get(), net::HttpByteRange::Bounded(offset, offset + length - 1),
      std::make_unique<MojoBlobReaderDelegate>(std::move(callback)),
      std::move(producer_handle));
}

}  // namespace content
