// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"

// A socket-like wrapper around Nearby Connections that allows for asynchronous
// reads and writes.
class NearbyConnection {
 public:
  using ReadCallback =
      base::OnceCallback<void(base::Optional<std::vector<uint8_t>> bytes)>;
  using WriteCallback = base::OnceCallback<void(bool result)>;

  virtual ~NearbyConnection() = default;

  // Reads a stream of bytes from the remote device. Invoke |callback| when
  // there is incoming data or when the socket is closed.
  virtual void Read(ReadCallback callback) = 0;

  // Writes an outgoing stream of bytes to the remote device asynchronously.
  // Invoke |callback| with True if successful, False if failed or socket is
  // closed.
  virtual void Write(std::vector<uint8_t> bytes, WriteCallback callback) = 0;

  // Closes the socket and disconnects from the remote device.
  virtual void Close() = 0;

  // Return True if the socket is closed, False otherwise.
  virtual bool IsClosed();

  // Listens to the socket being closed. Invoke |callback| when the socket is
  // closed.
  virtual void RegisterForDisconnection(base::OnceClosure callback) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTION_H_
