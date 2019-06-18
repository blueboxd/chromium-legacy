// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_origin_read_blocking_checker.h"

#include "base/callback.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "url/origin.h"

namespace content {

CrossOriginReadBlockingChecker::CrossOriginReadBlockingChecker(
    const network::ResourceRequest& request,
    const network::ResourceResponseHead& response,
    const url::Origin& request_initiator_site_lock,
    const storage::BlobDataHandle& blob_data_handle,
    base::OnceCallback<void(Result)> callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  network::CrossOriginReadBlocking::LogAction(
      network::CrossOriginReadBlocking::Action::kResponseStarted);
  corb_analyzer_ =
      std::make_unique<network::CrossOriginReadBlocking::ResponseAnalyzer>(
          request.url, request.request_initiator, response,
          request_initiator_site_lock, request.mode);
  if (corb_analyzer_->ShouldBlock()) {
    OnBlocked();
    return;
  }
  if (corb_analyzer_->needs_sniffing()) {
    StartSniffing(blob_data_handle);
    return;
  }
  DCHECK(corb_analyzer_->ShouldAllow());
  OnAllowed();
}

CrossOriginReadBlockingChecker::~CrossOriginReadBlockingChecker() = default;

int CrossOriginReadBlockingChecker::GetNetError() {
  DCHECK(blob_reader_);
  return blob_reader_->net_error();
}

void CrossOriginReadBlockingChecker::OnAllowed() {
  corb_analyzer_->LogAllowedResponse();
  std::move(callback_).Run(Result::kAllowed);
}

void CrossOriginReadBlockingChecker::OnBlocked() {
  corb_analyzer_->LogBlockedResponse();
  std::move(callback_).Run(corb_analyzer_->ShouldReportBlockedResponse()
                               ? Result::kBlocked_ShouldReport
                               : Result::kBlocked_ShouldNotReport);
}

void CrossOriginReadBlockingChecker::OnNetError() {
  std::move(callback_).Run(Result::kNetError);
}

void CrossOriginReadBlockingChecker::StartSniffing(
    const storage::BlobDataHandle& blob_data_handle) {
  blob_reader_ = blob_data_handle.CreateReader();
  const storage::BlobReader::Status size_status = blob_reader_->CalculateSize(
      base::BindOnce(&CrossOriginReadBlockingChecker::DidCalculateSize,
                     base::Unretained(this)));
  switch (size_status) {
    case storage::BlobReader::Status::NET_ERROR:
      OnNetError();
      return;
    case storage::BlobReader::Status::IO_PENDING:
      return;
    case storage::BlobReader::Status::DONE:
      DidCalculateSize(net::OK);
      return;
  }
}

void CrossOriginReadBlockingChecker::DidCalculateSize(int result) {
  size_t buf_size = net::kMaxBytesToSniff;
  if (buf_size > blob_reader_->total_size())
    buf_size = blob_reader_->total_size();
  buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(buf_size);
  int bytes_read;
  const storage::BlobReader::Status status = blob_reader_->Read(
      buffer_.get(), buf_size, &bytes_read,
      base::BindOnce(&CrossOriginReadBlockingChecker::OnReadComplete,
                     base::Unretained(this)));
  switch (status) {
    case storage::BlobReader::Status::NET_ERROR:
      OnNetError();
      return;
    case storage::BlobReader::Status::IO_PENDING:
      return;
    case storage::BlobReader::Status::DONE:
      OnReadComplete(bytes_read);
      return;
  }
}

void CrossOriginReadBlockingChecker::OnReadComplete(int bytes_read) {
  if (bytes_read != buffer_->size()) {
    OnNetError();
    return;
  }
  base::StringPiece data(buffer_->data(), bytes_read);
  corb_analyzer_->SniffResponseBody(data, 0);
  if (corb_analyzer_->ShouldBlock()) {
    OnBlocked();
    return;
  }
  OnAllowed();
}

}  // namespace content
