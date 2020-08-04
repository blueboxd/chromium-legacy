// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_

#include <queue>
#include <vector>

#include "chrome/browser/nearby_sharing/nearby_connection.h"

class FakeNearbyConnection : public NearbyConnection {
 public:
  FakeNearbyConnection();
  ~FakeNearbyConnection() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void RegisterForDisconnection(base::OnceClosure listener) override;

  void AppendReadableData(std::vector<uint8_t> bytes);

 private:
  void MaybeRunCallback();

  bool closed_ = false;
  ReadCallback callback_;
  std::queue<std::vector<uint8_t>> data_;
  std::vector<base::OnceClosure> disconnect_listeners_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTION_H_
