// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthUseNativeWinApi;
#endif  // defined(OS_WIN)

// Enable support for PIN-based user-verification.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthPINSupport;

// Enable support for resident keys.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthResidentKeys;

// Enable biometric enrollment in the security keys settings UI.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthBiometricEnrollment;
//
// Enable credential management in the security keys settings UI.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthCredentialManagement;

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
