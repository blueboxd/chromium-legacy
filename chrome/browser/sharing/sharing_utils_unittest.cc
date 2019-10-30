// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    sync_pb::SyncEnums_DeviceType device_type =
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
    base::SysInfo::HardwareInfo hardware_info = base::SysInfo::HardwareInfo()) {
  return std::make_unique<syncer::DeviceInfo>(
      id, name, "chrome_version", "user_agent", device_type, "device_id",
      hardware_info,
      /*last_updated_timestamp=*/base::Time::Now(),
      /*send_tab_to_self_receiving_enabled=*/false,
      syncer::DeviceInfo::SharingInfo(
          "fcm_token", "P256dh", "auth_secret",
          std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
              sync_pb::SharingSpecificFields::CLICK_TO_CALL}));
}
}  // namespace

TEST(SharingUtilsTest, GetSharingDeviceNames_AppleDevices) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "name", sync_pb::SyncEnums_DeviceType_TYPE_MAC,
      {"Apple Inc.", "MacbookPro1,1", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("MacbookPro1,1", names.full_name);
  EXPECT_EQ("MacbookPro", names.short_name);
}

TEST(SharingUtilsTest, GetSharingDeviceNames_ChromeOSDevices) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "name", sync_pb::SyncEnums_DeviceType_TYPE_CROS,
      {"google", "Chromebook", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("google Chromebook", names.full_name);
  EXPECT_EQ("google Chromebook", names.short_name);
}

TEST(SharingUtilsTest, GetSharingDeviceNames_AndroidPhones) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "name", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      {"google", "Pixel 2", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("google Phone Pixel 2", names.full_name);
  EXPECT_EQ("google Phone", names.short_name);
}

TEST(SharingUtilsTest, GetSharingDeviceNames_AndroidTablets) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "name", sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
      {"google", "Pixel Slate", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("google Tablet Pixel Slate", names.full_name);
  EXPECT_EQ("google Tablet", names.short_name);
}

TEST(SharingUtilsTest, GetSharingDeviceNames_Desktops) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "name", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      {"Dell", "BX123", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Dell Computer BX123", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}
