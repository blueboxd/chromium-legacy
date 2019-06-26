// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_COMMON_GCM_MESSAGE_H_
#define COMPONENTS_GCM_DRIVER_COMMON_GCM_MESSAGE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "components/gcm_driver/common/gcm_driver_export.h"

namespace gcm {

// Message data consisting of key-value pairs.
using MessageData = std::map<std::string, std::string>;

// Message to be delivered to the other party.
struct GCM_DRIVER_EXPORT OutgoingMessage {
  OutgoingMessage();
  OutgoingMessage(const OutgoingMessage& other);
  ~OutgoingMessage();

  // Message ID.
  std::string id;
  // In seconds.
  int time_to_live;
  MessageData data;

  static const int kMaximumTTL;
};

// Message being received from the other party.
struct GCM_DRIVER_EXPORT IncomingMessage {
  IncomingMessage();
  IncomingMessage(const IncomingMessage& other);
  ~IncomingMessage();

  MessageData data;
  std::string collapse_key;
  std::string sender_id;
  std::string message_id;
  std::string raw_data;

  // Whether the contents of the message have been decrypted, and are
  // available in |raw_data|.
  bool decrypted;
};

// Message to be delivered to the other party via Web Push.
struct GCM_DRIVER_EXPORT WebPushMessage {
  WebPushMessage();
  WebPushMessage(WebPushMessage&& other);
  ~WebPushMessage();

  // Message ID.
  std::string id;
  // In seconds.
  int time_to_live;
  std::string payload;

  static const int kMaximumTTL;

  DISALLOW_COPY_AND_ASSIGN(WebPushMessage);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_COMMON_GCM_MESSAGE_H_
