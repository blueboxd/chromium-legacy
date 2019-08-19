// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_CAPABILITY_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_CAPABILITY_H_

// Capabilities which a device can perform. These are stored in sync preferences
// when the device is registered, and the values should never be changed. When
// adding a new capability, the value should be '1 << (NEXT_FREE_BIT_ID)' and
// NEXT_FREE_BIT_ID should be incremented by one.
// NEXT_FREE_BIT_ID: 2
enum class SharingDeviceCapability {
  kNone = 0,
  kClickToCall = 1 << 0,
  kSharedClipboard = 1 << 1
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_CAPABILITY_H_
