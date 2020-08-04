// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"

FakeNearbyConnection::FakeNearbyConnection() = default;
FakeNearbyConnection::~FakeNearbyConnection() = default;

void FakeNearbyConnection::Read(ReadCallback callback) {
  DCHECK(!closed_);
  callback_ = std::move(callback);
  MaybeRunCallback();
}

void FakeNearbyConnection::Write(std::vector<uint8_t> bytes) {
  DCHECK(!closed_);
  NOTIMPLEMENTED();
}

void FakeNearbyConnection::Close() {
  DCHECK(!closed_);
  closed_ = true;
  if (callback_)
    std::move(callback_).Run(base::nullopt);
  for (auto& disconnect_listener : disconnect_listeners_)
    std::move(disconnect_listener).Run();
}

void FakeNearbyConnection::RegisterForDisconnection(
    base::OnceClosure listener) {
  DCHECK(!closed_);
  disconnect_listeners_.push_back(std::move(listener));
}

void FakeNearbyConnection::AppendReadableData(std::vector<uint8_t> bytes) {
  DCHECK(!closed_);
  data_.push(std::move(bytes));
  MaybeRunCallback();
}

void FakeNearbyConnection::MaybeRunCallback() {
  DCHECK(!closed_);
  if (!callback_ || data_.empty())
    return;
  auto item = std::move(data_.front());
  data_.pop();
  std::move(callback_).Run(std::move(item));
}
