// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/fake_local_device_info_provider.h"

#include "base/time/time.h"

FakeLocalDeviceInfoProvider::FakeLocalDeviceInfoProvider()
    : device_info_("id",
                   "name",
                   "chrome_version",
                   "user_agent",
                   sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                   "device_id",
                   /* last_updated_timestamp= */ base::Time::Now(),
                   /* send_tab_to_self_receiving_enabled= */ false) {}

FakeLocalDeviceInfoProvider::~FakeLocalDeviceInfoProvider() = default;

version_info::Channel FakeLocalDeviceInfoProvider::GetChannel() const {
  NOTIMPLEMENTED();
  return version_info::Channel::UNKNOWN;
}

const syncer::DeviceInfo* FakeLocalDeviceInfoProvider::GetLocalDeviceInfo()
    const {
  return &device_info_;
}

std::unique_ptr<FakeLocalDeviceInfoProvider::Subscription>
FakeLocalDeviceInfoProvider::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  NOTIMPLEMENTED();
  return nullptr;
}
