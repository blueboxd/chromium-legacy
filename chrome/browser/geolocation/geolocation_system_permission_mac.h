// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_

// System permission state.
enum class SystemPermissionStatus {
  kNotDetermined = 0,
  kDenied = 1,
  kAllowed = 2,
  kMaxValue = kAllowed
};

// This class is owned by the browser process and keeps track of the macOS
// location permissions for the browser.
class GeolocationSystemPermissionManager {
 public:
  virtual ~GeolocationSystemPermissionManager();
  static std::unique_ptr<GeolocationSystemPermissionManager> Create();
  virtual SystemPermissionStatus GetSystemPermission() = 0;
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_SYSTEM_PERMISSION_MAC_H_