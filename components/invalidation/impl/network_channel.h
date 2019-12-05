// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_
#define COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_

#include "base/callback.h"

namespace syncer {

using MessageCallback =
    base::RepeatingCallback<void(const std::string& payload,
                                 const std::string& private_topic,
                                 const std::string& public_topic,
                                 int64_t version)>;
using TokenCallback = base::RepeatingCallback<void(const std::string& message)>;

// Interface specifying the functionality of the network, required by
// invalidation client.
// TODO(crbug.com/1029481): Get rid of this interface; it has a single
// implementation and nothing refers to it directly.
class NetworkChannel {
 public:
  virtual ~NetworkChannel() {}

  // Sets the receiver to which messages from the data center will be delivered.
  virtual void SetMessageReceiver(MessageCallback incoming_receiver) = 0;

  // Sets the receiver to which FCM registration token will be delivered.
  virtual void SetTokenReceiver(TokenCallback incoming_receiver) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_
