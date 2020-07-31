// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"

FakeNearbyConnection::FakeNearbyConnection() = default;
FakeNearbyConnection::~FakeNearbyConnection() = default;

void FakeNearbyConnection::Read(ReadCallback callback) {
  callback_ = std::move(callback);
  MaybeRunCallback();
}

void FakeNearbyConnection::Write(std::vector<uint8_t> bytes,
                                 WriteCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnection::Close() {
  closed_ = true;
  if (callback_)
    std::move(callback_).Run(base::nullopt);
}

bool FakeNearbyConnection::IsClosed() const {
  return closed_;
}

void FakeNearbyConnection::RegisterForDisconnection(
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnection::AppendReadableData(std::vector<uint8_t> bytes) {
  data_.push(std::move(bytes));
  MaybeRunCallback();
}

void FakeNearbyConnection::MaybeRunCallback() {
  if (!callback_ || data_.empty())
    return;
  auto item = std::move(data_.front());
  data_.pop();
  std::move(callback_).Run(std::move(item));
}
