// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_
#define CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_

// This header contains types related to the security token PIN requests.

namespace chromeos {

// Type of the information asked from the user during a security token PIN
// request.
enum class SecurityTokenPinCodeType {
  kPin,
  kPuk,
};

// Error to be displayed in the security token PIN request.
enum class SecurityTokenPinErrorLabel {
  kNone,
  kUnknown,
  kInvalidPin,
  kInvalidPuk,
  kMaxAttemptsExceeded,
};

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_
