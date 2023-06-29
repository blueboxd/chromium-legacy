// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/pixel_buffer_transferer.h"

#include "base/check.h"
#include "base/logging.h"

namespace media {

PixelBufferTransferer::PixelBufferTransferer() {
  if (__builtin_available(macOS 10.8, *)) {
    OSStatus error =
        VTPixelTransferSessionCreate(nil, transfer_session_.InitializeInto());
    // There is no known way to make session creation fail, so we do not deal with
    // failures gracefully.
    CHECK(error == noErr) << "Creating a VTPixelTransferSession failed: "
                          << error;
  }
}

bool PixelBufferTransferer::TransferImage(CVPixelBufferRef source,
                                          CVPixelBufferRef destination) {
  DCHECK(source);
  DCHECK(destination);
  if (__builtin_available(macOS 10.8, *)) {
    OSStatus error = VTPixelTransferSessionTransferImage(transfer_session_,
                                                         source, destination);
    if (error == kVTPixelTransferNotSupportedErr) {
      // This source/destination transfer operation is not supported.
      return false;
    }
    CHECK(error == noErr)
        << "Unexpected VTPixelTransferSessionTransferImage error: " << error;
    return true;
  }
  return false;
}

PixelBufferTransferer::~PixelBufferTransferer() {
  if (__builtin_available(macOS 10.8, *)) {
    VTPixelTransferSessionInvalidate(transfer_session_);
  }
}

}  // namespace media
