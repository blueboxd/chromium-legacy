// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_

#include <string>

namespace vr {

// TODO(https://crbug.com/917526): add support for unloading the SDK.
bool LoadArCoreSdk(const std::string& libraryPath);

// Determines whether AR Core features are supported.
// TODO(https://crbug.com/924380): Currently, this is very simplistic. It should
// consider whether the device can support ARCore.
// Calling this method won't load AR Core SDK and does not depend on AR Core SDK
// to be loaded.
// Returns true if the AR Core usage is supported, false otherwise.
bool IsArCoreSupported();

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_SHIM_H_
