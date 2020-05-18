// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_H_
#define FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/strings/string_piece_forward.h"

namespace cast_streaming {

// TODO(crbug.com/1082821): Remove this file once the Cast Streaming Receiver is
// implemented as a separate component from WebEngine.

// Returns true if |origin| is the Cast Streaming MessagePort origin.
bool IsCastStreamingAppOrigin(base::StringPiece origin);

// Returns true if |message| contains a valid Cast Streaming Message.
bool IsValidCastStreamingMessage(const fuchsia::web::WebMessage& message);

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_PUBLIC_CAST_STREAMING_H_
