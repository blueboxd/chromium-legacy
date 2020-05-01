// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/udev.h"

#include "base/files/file_path.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "device/udev_linux/udev_loader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(UdevTest, DecodeString) {
  ASSERT_EQ("", UdevDecodeString(""));
  ASSERT_EQ("\\", UdevDecodeString("\\x5c"));
  ASSERT_EQ("\\x5", UdevDecodeString("\\x5"));
  ASSERT_EQ("049f", UdevDecodeString("049f"));
  ASSERT_EQ(
      "HD Pro Webcam C920", UdevDecodeString("HD\\x20Pro\\x20Webcam\\x20C920"));
  ASSERT_EQ("E-MU Systems,Inc.", UdevDecodeString("E-MU\\x20Systems\\x2cInc."));
}

TEST(UdevTest, Loader) {
  ASSERT_NE(nullptr, UdevLoader::Get());
}

TEST(UdevTest, GetSysAttrNoAttrs) {
  testing::FakeUdevLoader fake_udev;
  udev_device* device = fake_udev.AddFakeDevice("Foo", "/device/foo", {}, {});

  const std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetSysAttrSimple) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  attrs.emplace("attr", "attr value");
  udev_device* device =
      fake_udev.AddFakeDevice("Foo", "/device/foo", std::move(attrs), {});

  std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_EQ("attr value", attr_value);

  attr_value = UdevDeviceGetSysattrValue(device, "unknown attr");
  EXPECT_TRUE(attr_value.empty());
}

TEST(UdevTest, GetParent) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  udev_device* parent = fake_udev.AddFakeDevice("Foo", "/device/foo", {}, {});
  udev_device* device =
      fake_udev.AddFakeDevice("Foo", "/device/foo/bar", {}, {});

  EXPECT_EQ(parent, udev_device_get_parent(device));
  EXPECT_EQ(nullptr, udev_device_get_parent(parent));
}

TEST(UdevTest, GetSysAttrRecursiveOneLevel) {
  testing::FakeUdevLoader fake_udev;
  std::map<std::string, std::string> attrs;
  attrs.emplace("attr", "attr value");
  fake_udev.AddFakeDevice("Foo", "/device/foo", std::move(attrs), {});
  udev_device* device =
      fake_udev.AddFakeDevice("Foo", "/device/foo/bar", {}, {});

  // Don't find the attr on the current device.
  std::string attr_value = UdevDeviceGetSysattrValue(device, "attr");
  EXPECT_TRUE(attr_value.empty());

  // Find it when searching recursive.
  attr_value = UdevDeviceRecursiveGetSysattrValue(device, "attr");
  EXPECT_EQ("attr value", attr_value);
}

}  // namespace device
