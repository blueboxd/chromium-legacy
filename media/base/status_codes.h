// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATUS_CODES_H_
#define MEDIA_BASE_STATUS_CODES_H_

#include <cstdint>
#include <limits>
#include <ostream>

#include "media/base/media_export.h"

namespace media {

using StatusCodeType = int32_t;
// TODO(tmathmeyer, liberato, xhwang) These numbers are not yet finalized:
// DO NOT use them for reporting statistics, and DO NOT report them to any
// user-facing feature, including media log.

// Codes are grouped with a bitmask:
// 0xFFFFFFFF
//   └─┬┘├┘└┴ enumeration within the group
//     │ └─ group code
//     └─ reserved for now
// 256 groups is more than anyone will ever need on a computer.
enum class StatusCode : StatusCodeType {
  kOk = 0,

  // Decoder Errors: 0x01
  kDecoderInitializeNeverCompleted = 0x00000101,
  kDecoderFailedDecode = 0x00000102,
  kDecoderUnsupportedProfile = 0x00000103,
  kDecoderUnsupportedCodec = 0x00000104,
  kDecoderUnsupportedConfig = 0x00000105,
  kEncryptedContentUnsupported = 0x00000106,
  kClearContentUnsupported = 0x00000107,
  kDecoderMissingCdmForEncryptedContent = 0x00000108,
  kDecoderFailedInitialization = 0x00000109,
  kDecoderCantChangeCodec = 0x0000010A,
  kDecoderFailedCreation = 0x0000010B,
  kInitializationUnspecifiedFailure = 0x0000010C,

  // Windows Errors: 0x02
  kWindowsWrappedHresult = 0x00000201,
  kWindowsApiNotAvailible = 0x00000202,
  kWindowsD3D11Error = 0x00000203,

  // D3D11VideoDecoder Errors: 0x03
  kCantMakeContextCurrent = 0x00000301,
  kCantPostTexture = 0x00000302,
  kCantPostAcquireStream = 0x00000303,
  kCantCreateEglStream = 0x00000304,
  kCantCreateEglStreamConsumer = 0x00000305,
  kCantCreateEglStreamProducer = 0x00000306,

  // MojoDecoder Errors: 0x04
  kMojoDecoderNoWrappedDecoder = 0x00000401,
  kMojoDecoderStoppedBeforeInitDone = 0x00000402,
  kMojoDecoderUnsupported = 0x00000403,
  kMojoDecoderNoConnection = 0x00000404,
  kMojoDecoderDeletedWithoutInitialization = 0x00000405,

  // Chromeos Errors: 0x05
  kChromeOSVideoDecoderNoDecoders = 0x00000501,
  kV4l2NoDevice = 0x00000502,
  kV4l2FailedToStopStreamQueue = 0x00000503,
  kV4l2NoDecoder = 0x00000504,
  kV4l2FailedFileCapabilitiesCheck = 0x00000505,
  kV4l2FailedResourceAllocation = 0x00000506,
  kV4l2BadFormat = 0x00000507,
  kVaapiReinitializedDuringDecode = 0x00000508,
  kVaapiFailedAcceleratorCreation = 0x00000509,

  // Encoder Error: 0x06
  kEncoderInitializeNeverCompleted = 0x00000601,
  kEncoderInitializeTwice = 0x00000602,
  kEncoderFailedEncode = 0x00000603,
  kEncoderUnsupportedProfile = 0x00000604,
  kEncoderUnsupportedCodec = 0x00000605,
  kEncoderUnsupportedConfig = 0x00000606,
  kEncoderInitializationError = 0x00000607,

  // Special codes
  kGenericErrorPleaseRemove = 0x79999999,
  kCodeOnlyForTesting = std::numeric_limits<StatusCodeType>::max(),
  kMaxValue = kCodeOnlyForTesting,
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os, const StatusCode& code);

}  // namespace media

#endif  // MEDIA_BASE_STATUS_CODES_H_
