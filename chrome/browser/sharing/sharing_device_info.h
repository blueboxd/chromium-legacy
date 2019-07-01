// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

// Capabilities of which a device can perform.
enum class SharingDeviceCapability { kNone = 0 };

// A class that holds information regarding the properties of a device.
class SharingDeviceInfo {
 public:
  SharingDeviceInfo(const std::string& guid,
                    const std::string& human_readable_name,
                    base::Time last_online_timestamp,
                    int capabilities);
  ~SharingDeviceInfo();
  SharingDeviceInfo(SharingDeviceInfo&& other);

  // Unique identifier for the device.
  const std::string& guid() const;

  // A human readable name of the device.
  const std::string& human_readable_name() const;

  // Returns the time at which this device was last online.
  base::Time last_online_timestamp() const;

  // Gets a bitmask of available capabilities of the device, defined in
  // SharingDeviceCapability enum.
  int capabilities() const;

 private:
  const std::string guid_;

  const std::string human_readable_name_;

  const base::Time last_online_timestamp_;

  int capabilities_;

  DISALLOW_COPY_AND_ASSIGN(SharingDeviceInfo);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_
