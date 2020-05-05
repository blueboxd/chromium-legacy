// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_device.h"

#include <wayland-server-core.h>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"

namespace wl {

namespace {

void DataDeviceStartDrag(wl_client* client,
                         wl_resource* resource,
                         wl_resource* source,
                         wl_resource* origin,
                         wl_resource* icon,
                         uint32_t serial) {
  NOTIMPLEMENTED();
}

void DataDeviceSetSelection(wl_client* client,
                            wl_resource* resource,
                            wl_resource* data_source,
                            uint32_t serial) {
  GetUserDataAs<TestDataDevice>(resource)->SetSelection(
      data_source ? GetUserDataAs<TestDataSource>(data_source) : nullptr,
      serial);
}

void DataDeviceRelease(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

}  // namespace

const struct wl_data_device_interface kTestDataDeviceImpl = {
    &DataDeviceStartDrag, &DataDeviceSetSelection, &DataDeviceRelease};

TestDataDevice::TestDataDevice(wl_resource* resource, wl_client* client)
    : ServerObject(resource), client_(client) {}

TestDataDevice::~TestDataDevice() {}

void TestDataDevice::SetSelection(TestDataSource* data_source,
                                  uint32_t serial) {
  NOTIMPLEMENTED();
}

TestDataOffer* TestDataDevice::OnDataOffer() {
  wl_resource* data_offer_resource = CreateResourceWithImpl<TestDataOffer>(
      client_, &wl_data_offer_interface, wl_resource_get_version(resource()),
      &kTestDataOfferImpl, 0);
  data_offer_ = GetUserDataAs<TestDataOffer>(data_offer_resource);
  wl_data_device_send_data_offer(resource(), data_offer_resource);

  return GetUserDataAs<TestDataOffer>(data_offer_resource);
}

void TestDataDevice::OnEnter(uint32_t serial,
                             wl_resource* surface,
                             wl_fixed_t x,
                             wl_fixed_t y,
                             TestDataOffer* data_offer) {
  wl_data_device_send_enter(resource(), serial, surface, x, y,
                            data_offer->resource());
}

void TestDataDevice::OnLeave() {
  wl_data_device_send_leave(resource());
}

void TestDataDevice::OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  wl_data_device_send_motion(resource(), time, x, y);
}

void TestDataDevice::OnDrop() {
  wl_data_device_send_drop(resource());
}

void TestDataDevice::OnSelection(TestDataOffer* data_offer) {
  wl_data_device_send_selection(resource(), data_offer->resource());
}

}  // namespace wl
