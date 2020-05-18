// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/public/cast_streaming.h"

#include "base/strings/string_piece.h"

namespace cast_streaming {

bool IsCastStreamingAppOrigin(base::StringPiece origin) {
  constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
  return origin == kCastStreamingMessagePortOrigin;
}

bool IsValidCastStreamingMessage(const fuchsia::web::WebMessage& message) {
  // |message| should contain exactly one OutgoingTransferrable, with a single
  // MessagePort.
  return message.has_outgoing_transfer() &&
         message.outgoing_transfer().size() == 1u &&
         message.outgoing_transfer()[0].is_message_port();
}

}  // namespace cast_streaming
