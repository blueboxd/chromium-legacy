// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_ERROR_CODES_H_
#define MEDIA_BASE_MEDIA_ERROR_CODES_H_

namespace media {

// NOTE: These numbers are still subject to change.  Do not use for things like
// UMA yet!

// Codes are grouped with a bitmask:
// 0xFFFFFFFF
//   └─┬┘├┘└┴ enumeration within the group
//     │ └─ group code
//     └─ reserved for now
// 256 groups is more than anyone will ever need on a computer.
enum class ErrorCode : uint32_t {
  kOk = 0,

  // Decoder Errors: 0x01
  kDecoderInitializeNeverCompleted = 0x00000101,
  kDecoderFailedDecode = 0x00000102,
  kDecoderUnsupportedProfile = 0x00000103,
  kDecoderUnsupportedCodec = 0x00000104,

  // Windows Errors: 0x02
  kWindowsWrappedHresult = 0x00000201,
  kWindowsApiNotAvailible = 0x00000202,

  // D3D11VideoDecoder Errors: 0x03
  kCannotMakeContextCurrent = 0x00000301,
  kCouldNotPostTexture = 0x00000302,
  kCouldNotPostAcquireStream = 0x00000303,

  kCodeOnlyForTesting = std::numeric_limits<uint32_t>::max(),
  kMaxValue = kCodeOnlyForTesting,
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_ERROR_CODES_H_
