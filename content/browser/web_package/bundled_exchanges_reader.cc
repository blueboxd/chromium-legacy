// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_reader.h"

#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

BundledExchangesReader::SharedFile::SharedFile(const base::FilePath& file_path)
    : file_path_(file_path) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& file_path) -> std::unique_ptr<base::File> {
            return std::make_unique<base::File>(
                file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
          },
          file_path_),
      base::BindOnce(&SharedFile::SetFile, base::RetainedRef(this)));
}

void BundledExchangesReader::SharedFile::DuplicateFile(
    base::OnceCallback<void(base::File)> callback) {
  // Basically this interface expects this method is called at most once. Have
  // a DCHECK for the case that does not work for a clear reason, just in case.
  // The call site also have another DCHECK for external callers not to cause
  // such problematic cases.
  DCHECK(duplicate_callback_.is_null());
  duplicate_callback_ = std::move(callback);

  if (file_)
    SetFile(std::move(file_));
}

base::File* BundledExchangesReader::SharedFile::operator->() {
  DCHECK(file_);
  return file_.get();
}

BundledExchangesReader::SharedFile::~SharedFile() {
  // Move the last reference to |file_| that leads an internal blocking call
  // that is not permitted here.
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([](std::unique_ptr<base::File> file) {},
                     std::move(file_)));
}

void BundledExchangesReader::SharedFile::SetFile(
    std::unique_ptr<base::File> file) {
  file_ = std::move(file);

  if (duplicate_callback_.is_null())
    return;

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::File* file) -> base::File { return file->Duplicate(); },
          file_.get()),
      std::move(duplicate_callback_));
}

class BundledExchangesReader::SharedFileDataSource final
    : public mojo::DataPipeProducer::DataSource {
 public:
  SharedFileDataSource(scoped_refptr<BundledExchangesReader::SharedFile> file,
                       uint64_t offset,
                       uint64_t length)
      : file_(std::move(file)), offset_(offset), length_(length) {
    error_ = mojo::FileDataSource::ConvertFileErrorToMojoResult(
        (*file_)->error_details());

    // base::File::Read takes int64_t as an offset. So, offset + length should
    // not overflow in int64_t.
    uint64_t max_offset;
    if (!base::CheckAdd(offset, length).AssignIfValid(&max_offset) ||
        (std::numeric_limits<int64_t>::max() < max_offset)) {
      error_ = MOJO_RESULT_INVALID_ARGUMENT;
    }
  }

 private:
  // Implements mojo::DataPipeProducer::DataSource. Following methods are called
  // on a blockable sequenced task runner.
  uint64_t GetLength() const override { return length_; }
  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    ReadResult result;
    result.result = error_;

    if (length_ < offset)
      result.result = MOJO_RESULT_INVALID_ARGUMENT;

    if (result.result != MOJO_RESULT_OK)
      return result;

    uint64_t readable_size = length_ - offset;
    uint64_t writable_size = buffer.size();
    uint64_t copyable_size =
        std::min(std::min(readable_size, writable_size),
                 static_cast<uint64_t>(std::numeric_limits<int>::max()));

    int bytes_read =
        (*file_)->Read(offset_ + offset, buffer.data(), copyable_size);
    if (bytes_read < 0) {
      result.result = mojo::FileDataSource::ConvertFileErrorToMojoResult(
          (*file_)->GetLastFileError());
    } else {
      result.bytes_read = bytes_read;
    }
    return result;
  }

  scoped_refptr<BundledExchangesReader::SharedFile> file_;
  MojoResult error_;
  const uint64_t offset_;
  const uint64_t length_;

  DISALLOW_COPY_AND_ASSIGN(SharedFileDataSource);
};

BundledExchangesReader::BundledExchangesReader(
    const BundledExchangesSource& source)
    : file_(base::MakeRefCounted<SharedFile>(source.file_path)) {}

BundledExchangesReader::~BundledExchangesReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BundledExchangesReader::ReadMetadata(MetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metadata_ready_);

  file_->DuplicateFile(
      base::BindOnce(&BundledExchangesReader::ReadMetadataInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BundledExchangesReader::ReadResponse(const GURL& url,
                                          ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  auto it = entries_.find(url);
  if (it == entries_.end()) {
    PostTask(FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  parser_.ParseResponse(
      it->second->response_offset, it->second->response_length,
      base::BindOnce(&BundledExchangesReader::OnResponseParsed,
                     base::Unretained(this), std::move(callback)));
}

void BundledExchangesReader::ReadResponseBody(
    data_decoder::mojom::BundleResponsePtr response,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    BodyCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  auto data_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* raw_producer = data_producer.get();
  raw_producer->Write(
      std::make_unique<SharedFileDataSource>(file_, response->payload_offset,
                                             response->payload_length),
      base::BindOnce([](std::unique_ptr<mojo::DataPipeProducer> producer,
                        BodyCompletionCallback callback,
                        MojoResult result) { std::move(callback).Run(result); },
                     std::move(data_producer), std::move(callback)));
}

bool BundledExchangesReader::HasEntry(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  return entries_.contains(url);
}

const GURL& BundledExchangesReader::GetStartURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  return start_url_;
}

void BundledExchangesReader::SetBundledExchangesParserFactoryForTesting(
    std::unique_ptr<data_decoder::mojom::BundledExchangesParserFactory>
        factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  parser_.SetBundledExchangesParserFactoryForTesting(std::move(factory));
}

void BundledExchangesReader::ReadMetadataInternal(MetadataCallback callback,
                                                  base::File file) {
  parser_.OpenFile(
      ServiceManagerConnection::GetForProcess()
          ? ServiceManagerConnection::GetForProcess()->GetConnector()
          : nullptr,
      std::move(file));

  parser_.ParseMetadata(
      base::BindOnce(&BundledExchangesReader::OnMetadataParsed,
                     base::Unretained(this), std::move(callback)));
}

void BundledExchangesReader::OnMetadataParsed(
    MetadataCallback callback,
    data_decoder::mojom::BundleMetadataPtr metadata,
    const base::Optional<std::string>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metadata_ready_);

  metadata_ready_ = true;

  if (metadata) {
    // TODO(crbug.com/966753): Use the primary (fallback) URL that the new
    // BundledExchanges format defines.
    if (!metadata->index.empty())
      start_url_ = metadata->index[0]->request_url;

    for (auto& item : metadata->index)
      entries_.insert(std::make_pair(item->request_url, std::move(item)));
  }
  std::move(callback).Run(error);
}

void BundledExchangesReader::OnResponseParsed(
    ResponseCallback callback,
    data_decoder::mojom::BundleResponsePtr response,
    const base::Optional<std::string>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_ready_);

  // TODO(crbug.com/966753): Handle |error|.
  std::move(callback).Run(std::move(response));
}

}  // namespace content
