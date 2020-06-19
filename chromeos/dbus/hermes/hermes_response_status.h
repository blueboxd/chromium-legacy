// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
#define CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_

#include <string>

#include "base/callback.h"

namespace chromeos {

// Enum values the represent response status of hermes client method calls.
enum class HermesResponseStatus {
  kSuccess,
  kErrorAlreadyDisabled,
  kErrorAlreadyEnabled,
  kErrorInvalidActivationCode,
  kErrorInvalidIccid,
  kErrorInvalidParameter,
  kErrorNeedConfirmationCode,
  kErrorSendNotificationFailure,
  kErrorTestProfileInProd,
  kErrorUnknown,
  kErrorUnsupported,
  kErrorWrongState,
  kErrorInvalidResponse,
  kErrorNoResponse,
};

// Callback that receives only a HermesResponseStatus.
using HermesResponseCallback =
    base::OnceCallback<void(HermesResponseStatus status)>;

// Returns the HermesResponseStatus corresponding to the
// given dbus_|error_name|.
HermesResponseStatus HermesResponseStatusFromErrorName(
    const std::string& error_name);

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HERMES_HERMES_RESPONSE_STATUS_H_
