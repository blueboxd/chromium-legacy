// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_
#define COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_

#include "base/callback.h"

namespace syncer {

// See SetMessageReceiver below.
//   payload - additional info specific to the invalidation
//   private_topic - the internal (to FCM) representation for the public topic
//   public_topic - the topic which was invalidated, e.g. in case of Chrome
//                  Sync it'll be BOOKMARK or PASSWORD
//   version - version number of the invalidation
using MessageCallback =
    base::RepeatingCallback<void(const std::string& payload,
                                 const std::string& private_topic,
                                 const std::string& public_topic,
                                 int64_t version)>;

using TokenCallback = base::RepeatingCallback<void(const std::string& token)>;

// Interface specifying the functionality of the network, required by
// invalidation client.
// TODO(crbug.com/1029481): Get rid of this interface; it has a single
// implementation and nothing refers to it directly.
class NetworkChannel {
 public:
  virtual ~NetworkChannel() {}

  // Sets the receiver to which messages from the data center will be delivered.
  // The callback will be invoked whenever an invalidation message is received
  // from FCM. It is *not* guaranteed to be invoked exactly once or in-order
  // (with respect to the invalidation's version number).
  virtual void SetMessageReceiver(MessageCallback incoming_receiver) = 0;

  // Sets the receiver to which FCM registration token will be delivered.
  // The callback will be invoked whenever a new InstanceID token becomes
  // available.
  virtual void SetTokenReceiver(TokenCallback incoming_receiver) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_NETWORK_CHANNEL_H_
