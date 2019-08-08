// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_BUFFER_PARAMS_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_BUFFER_PARAMS_H_

#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zwp_linux_buffer_params_v1_interface
    kMockZwpLinuxBufferParamsV1Impl;

class MockZwpLinuxDmabufV1;

// Manage zwp_linux_buffer_params_v1
class MockZwpLinuxBufferParamsV1 : public ServerObject {
 public:
  MockZwpLinuxBufferParamsV1(wl_resource* resource);
  ~MockZwpLinuxBufferParamsV1();

  MOCK_METHOD2(Destroy, void(wl_client* client, wl_resource* resource));
  MOCK_METHOD8(Add,
               void(wl_client* client,
                    wl_resource* resource,
                    int32_t fd,
                    uint32_t plane_idx,
                    uint32_t offset,
                    uint32_t stride,
                    uint32_t modifier_hi,
                    uint32_t modifier_lo));
  MOCK_METHOD6(Create,
               void(wl_client* client,
                    wl_resource* resource,
                    int32_t width,
                    int32_t height,
                    uint32_t format,
                    uint32_t flags));
  MOCK_METHOD7(CreateImmed,
               void(wl_client* client,
                    wl_resource* resource,
                    uint32_t buffer_id,
                    int32_t width,
                    int32_t height,
                    uint32_t format,
                    uint32_t flags));

  wl_resource* buffer_resource() const { return buffer_resource_; }

  void SetZwpLinuxDmabuf(MockZwpLinuxDmabufV1* linux_dmabuf);

  void SetBufferResource(wl_resource* resource);

  std::vector<base::ScopedFD> fds_;

 private:
  // Non-owned pointer to the linux dmabuf object, which created this params
  // resource and holds a pointer to it. On destruction, must notify it about
  // going out of scope.
  MockZwpLinuxDmabufV1* linux_dmabuf_ = nullptr;

  // A buffer resource, which is created on Create or CreateImmed call. Can be
  // null if not created/failed to be created.
  wl_resource* buffer_resource_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockZwpLinuxBufferParamsV1);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_BUFFER_PARAMS_H_
