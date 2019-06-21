// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob_data_handle.h"

#include "base/bind.h"
#include "net/base/net_errors.h"

namespace storage {

FakeBlobDataHandle::FakeBlobDataHandle(std::string body_data,
                                       std::string side_data)
    : body_data_(std::move(body_data)), side_data_(std::move(side_data)) {}

void FakeBlobDataHandle::EnableDelayedReading() {
  delayed_reading_ = true;
}

bool FakeBlobDataHandle::HasPendingReadCallbacks() const {
  return !pending_read_callbacks_.empty();
}

void FakeBlobDataHandle::RunPendingReadCallbacks() {
  std::vector<base::OnceClosure> list = std::move(pending_read_callbacks_);
  for (auto& cb : list) {
    std::move(cb).Run();
  }
}

uint64_t FakeBlobDataHandle::GetSize() const {
  return body_data_.size();
}

int FakeBlobDataHandle::Read(scoped_refptr<net::IOBuffer> dst_buffer,
                             uint64_t src_offset,
                             int bytes_to_read,
                             base::OnceCallback<void(int)> callback) {
  if (src_offset >= body_data_.size())
    return net::ERR_FAILED;

  int num_bytes =
      std::min(static_cast<int>(body_data_.size() - src_offset), bytes_to_read);
  memcpy(dst_buffer->data(), body_data_.data() + src_offset, num_bytes);

  if (delayed_reading_)
    return PendingCallback(std::move(callback), num_bytes);

  return num_bytes;
}

uint64_t FakeBlobDataHandle::GetSideDataSize() const {
  return side_data_.size();
}

int FakeBlobDataHandle::ReadSideData(scoped_refptr<net::IOBuffer> dst_buffer,
                                     base::OnceCallback<void(int)> callback) {
  memcpy(dst_buffer->data(), side_data_.data(), side_data_.size());

  if (delayed_reading_)
    return PendingCallback(std::move(callback), side_data_.size());

  return side_data_.size();
}

FakeBlobDataHandle::~FakeBlobDataHandle() = default;

int FakeBlobDataHandle::PendingCallback(base::OnceCallback<void(int)> callback,
                                        int result) {
  pending_read_callbacks_.push_back(
      base::BindOnce(std::move(callback), result));
  return net::ERR_IO_PENDING;
}

}  // namespace storage
