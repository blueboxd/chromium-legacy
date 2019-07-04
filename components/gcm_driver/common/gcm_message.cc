// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/common/gcm_message.h"

namespace gcm {

// static
const int OutgoingMessage::kMaximumTTL = 24 * 60 * 60;  // 1 day.
const int WebPushMessage::kMaximumTTL = 24 * 60 * 60;   // 1 day.

OutgoingMessage::OutgoingMessage() : time_to_live(kMaximumTTL) {}

OutgoingMessage::OutgoingMessage(const OutgoingMessage& other) = default;

OutgoingMessage::~OutgoingMessage() = default;

IncomingMessage::IncomingMessage() : decrypted(false) {}

IncomingMessage::IncomingMessage(const IncomingMessage& other) = default;

IncomingMessage::~IncomingMessage() = default;

WebPushMessage::WebPushMessage() : time_to_live(kMaximumTTL) {}

WebPushMessage::WebPushMessage(WebPushMessage&& other) = default;

WebPushMessage::~WebPushMessage() = default;

WebPushMessage& WebPushMessage::operator=(WebPushMessage&& other) = default;

}  // namespace gcm
