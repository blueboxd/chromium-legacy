// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_BROWSER_H_
#define CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_BROWSER_H_

namespace content {

// Turn the browser process into web test mode.
void EnableBrowserWebTestMode();

// Sets the scan duration to reflect the given setting.
enum class BluetoothTestScanDurationSetting {
  kImmediateTimeout,  // Set the scan duration to 0 seconds.
  kNeverTimeout,  // Set the scan duration to base::TimeDelta::Max() seconds.
};
void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_TEST_SUPPORT_BROWSER_H_
