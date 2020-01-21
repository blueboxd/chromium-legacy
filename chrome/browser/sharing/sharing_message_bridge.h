// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_H_

#include <memory>

#include "components/sync/protocol/sharing_message_specifics.pb.h"

// Class to provide an interface to send sharing messages using Sync.
class SharingMessageBridge {
 public:
  // TODO(crbug.com/1034930): take callbacks once commit error propagation back
  // to the bridge is implemented.
  virtual void SendSharingMessage(
      std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics) = 0;

  virtual ~SharingMessageBridge() = default;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_H_
