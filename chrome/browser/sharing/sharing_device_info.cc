// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_info.h"

SharingDeviceInfo::SharingDeviceInfo(const std::string& guid,
                                     const std::string& human_readable_name,
                                     base::Time last_online_timestamp,
                                     int capabilities)
    : guid_(guid),
      human_readable_name_(human_readable_name),
      last_online_timestamp_(last_online_timestamp),
      capabilities_(capabilities) {}

SharingDeviceInfo::~SharingDeviceInfo() = default;

SharingDeviceInfo::SharingDeviceInfo(SharingDeviceInfo&& other) = default;

const std::string& SharingDeviceInfo::guid() const {
  return guid_;
}

const std::string& SharingDeviceInfo::human_readable_name() const {
  return human_readable_name_;
}

base::Time SharingDeviceInfo::last_online_timestamp() const {
  return last_online_timestamp_;
}

int SharingDeviceInfo::capabilities() const {
  return capabilities_;
}
