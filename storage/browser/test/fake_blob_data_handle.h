// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_
#define STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_

#include <string>

#include "base/callback.h"
#include "storage/browser/blob/blob_data_item.h"

namespace storage {

class FakeBlobDataHandle : public BlobDataItem::DataHandle {
 public:
  FakeBlobDataHandle(std::string body_data, std::string side_data);

  void EnableDelayedReading();
  bool HasPendingReadCallbacks() const;
  void RunPendingReadCallbacks();

  uint64_t GetSize() const override;
  int Read(scoped_refptr<net::IOBuffer> dst_buffer,
           uint64_t src_offset,
           int bytes_to_read,
           base::OnceCallback<void(int)> callback) override;

  uint64_t GetSideDataSize() const override;
  int ReadSideData(scoped_refptr<net::IOBuffer> dst_buffer,
                   base::OnceCallback<void(int)> callback) override;

 private:
  ~FakeBlobDataHandle() override;

  int PendingCallback(base::OnceCallback<void(int)> callback, int result);

  const std::string body_data_;
  const std::string side_data_;
  std::vector<base::OnceClosure> pending_read_callbacks_;
  bool delayed_reading_ = false;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FAKE_BLOB_DATA_HANDLE_H_
