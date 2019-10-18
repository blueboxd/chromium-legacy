// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/data_url_blob_reader.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"

namespace {

// Class for reading data from a BlobReader. Once blob reading completes,
// it constructs a data URL from the data and calls the callback
// given by the caller. All the operations are on the IO thread.
class BlobReadJob : public base::RefCounted<BlobReadJob> {
 public:
  BlobReadJob(content::DataURLBlobReader::ReadCompletionCallback
                  read_completion_callback,
              std::unique_ptr<storage::BlobReader> blob_reader);

  // Called to start the job.
  void Start();

 private:
  friend class base::RefCounted<BlobReadJob>;
  ~BlobReadJob();

  // Called when blob size is calculated.
  void OnSizeCalculated(int result_code);

  // Called when all the data is read.
  void OnReadComplete(int size);

  content::DataURLBlobReader::ReadCompletionCallback read_completion_callback_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;

  DISALLOW_COPY_AND_ASSIGN(BlobReadJob);
};

BlobReadJob::BlobReadJob(
    content::DataURLBlobReader::ReadCompletionCallback read_completion_callback,
    std::unique_ptr<storage::BlobReader> blob_reader)
    : read_completion_callback_(std::move(read_completion_callback)),
      blob_reader_(std::move(blob_reader)) {}

BlobReadJob::~BlobReadJob() = default;

void BlobReadJob::Start() {
  const storage::BlobReader::Status size_status = blob_reader_->CalculateSize(
      base::BindOnce(&BlobReadJob::OnSizeCalculated, this));
  switch (size_status) {
    case storage::BlobReader::Status::NET_ERROR:
      std::move(read_completion_callback_).Run(GURL());
      return;
    case storage::BlobReader::Status::IO_PENDING:
      return;
    case storage::BlobReader::Status::DONE:
      OnSizeCalculated(net::OK);
      return;
  }
}

void BlobReadJob::OnSizeCalculated(int result_code) {
  if (result_code != net::OK) {
    std::move(read_completion_callback_).Run(GURL());
    return;
  }
  io_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(
      static_cast<size_t>(blob_reader_->total_size()));
  int bytes_read;
  storage::BlobReader::Status read_status = blob_reader_->Read(
      io_buffer_.get(), blob_reader_->total_size(), &bytes_read,
      base::BindOnce(&BlobReadJob::OnReadComplete, this));
  switch (read_status) {
    case storage::BlobReader::Status::NET_ERROR:
      std::move(read_completion_callback_).Run(GURL());
      return;
    case storage::BlobReader::Status::IO_PENDING:
      return;
    case storage::BlobReader::Status::DONE:
      OnReadComplete(bytes_read);
      return;
  }
}

void BlobReadJob::OnReadComplete(int size) {
  base::StringPiece url_data = base::StringPiece(io_buffer_->data(), size);
  GURL url = base::StartsWith(url_data, "data:", base::CompareCase::SENSITIVE)
                 ? GURL(url_data)
                 : GURL();
  std::move(read_completion_callback_).Run(std::move(url));
}

}  // namespace

namespace content {

// static
void DataURLBlobReader::ReadDataURLFromBlob(
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
    DataURLBlobReader::ReadCompletionCallback read_completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_refptr<BlobReadJob> blob_read_job = base::MakeRefCounted<BlobReadJob>(
      std::move(read_completion_callback), blob_data_handle->CreateReader());
  blob_read_job->Start();
}

}  // namespace content
