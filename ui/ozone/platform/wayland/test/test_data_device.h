// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_

#include <wayland-server-protocol.h>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct wl_data_device_interface kTestDataDeviceImpl;

class TestDataOffer;
class TestDataSource;

class TestDataDevice : public ServerObject {
 public:
  TestDataDevice(wl_resource* resource, wl_client* client);
  ~TestDataDevice() override;

  void SetSelection(TestDataSource* data_source, uint32_t serial);

  TestDataOffer* OnDataOffer();
  void OnEnter(uint32_t serial,
               wl_resource* surface,
               wl_fixed_t x,
               wl_fixed_t y,
               TestDataOffer* data_offer);
  void OnLeave();
  void OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void OnDrop();
  void OnSelection(TestDataOffer* data_offer);

 private:
  TestDataOffer* data_offer_;
  wl_client* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestDataDevice);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
