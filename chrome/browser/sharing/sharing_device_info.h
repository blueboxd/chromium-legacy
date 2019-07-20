// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_
#define CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/protocol/sync.pb.h"

// Capabilities which a device can perform. These are stored in sync preferences
// when the device is registered, and the values should never be changed. When
// adding a new capability, the value should be '1 << (NEXT_FREE_BIT_ID)' and
// NEXT_FREE_BIT_ID should be incremented by one.
// NEXT_FREE_BIT_ID: 1
enum class SharingDeviceCapability { kNone = 0, kTelephony = 1 << 0 };

// A class that holds information regarding the properties of a device.
class SharingDeviceInfo {
 public:
  SharingDeviceInfo(const std::string& guid,
                    const base::string16& human_readable_name,
                    sync_pb::SyncEnums::DeviceType device_type,
                    base::Time last_online_timestamp,
                    int capabilities);
  ~SharingDeviceInfo();
  SharingDeviceInfo(SharingDeviceInfo&& other);

  // Unique identifier for the device.
  const std::string& guid() const;

  // A human readable name of the device.
  const base::string16& human_readable_name() const;

  // Type of the device whether it is a phone, tablet or desktop.
  sync_pb::SyncEnums::DeviceType device_type() const;

  // Returns the time at which this device was last online.
  base::Time last_online_timestamp() const;

  // Gets a bitmask of available capabilities of the device, defined in
  // SharingDeviceCapability enum.
  int capabilities() const;

 private:
  const std::string guid_;

  const base::string16 human_readable_name_;

  const sync_pb::SyncEnums::DeviceType device_type_;

  const base::Time last_online_timestamp_;

  int capabilities_;

  DISALLOW_COPY_AND_ASSIGN(SharingDeviceInfo);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_DEVICE_INFO_H_
