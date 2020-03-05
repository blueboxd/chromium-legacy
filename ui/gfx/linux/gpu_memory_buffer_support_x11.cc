// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"

#include <fcntl.h>
#include <xcb/dri3.h>
#include <xcb/xcb.h>

#include <memory>

#include "base/debug/crash_logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/linux/gbm_wrapper.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

// Obtain an authenticated DRM fd from X11 and create a GbmDevice with it.
std::unique_ptr<ui::GbmDevice> CreateX11GbmDevice() {
  XDisplay* display = gfx::GetXDisplay();
  // |display| may be nullptr in headless mode.
  if (!display)
    return nullptr;
  xcb_connection_t* connection = XGetXCBConnection(display);
  DCHECK(connection);

  const auto* dri3_extension = xcb_get_extension_data(connection, &xcb_dri3_id);
  if (!dri3_extension || !dri3_extension->present)
    return nullptr;

  // Let the X11 server know the DRI3 client version. This is required to use
  // the DRI3 extension. We don't care about the returned server version because
  // we only use features from the original DRI3 interface.
  auto version_cookie = xcb_dri3_query_version(
      connection, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);
  auto* version_reply =
      xcb_dri3_query_version_reply(connection, version_cookie, nullptr);
  if (!version_reply)
    return nullptr;
  free(version_reply);

  // Obtain an authenticated DRM fd.
  auto open_cookie =
      xcb_dri3_open(connection, DefaultRootWindow(display), x11::None);
  auto* open_reply = xcb_dri3_open_reply(connection, open_cookie, nullptr);
  if (!open_reply)
    return nullptr;
  if (open_reply->nfd != 1)
    return nullptr;
  int fd = xcb_dri3_open_reply_fds(connection, open_reply)[0];
  free(open_reply);

  if (fd < 0)
    return nullptr;
  if (HANDLE_EINTR(fcntl(fd, F_SETFD, FD_CLOEXEC)) == -1)
    return nullptr;

  return ui::CreateGbmDevice(fd);
}

std::vector<gfx::BufferUsageAndFormat> CreateSupportedConfigList(
    ui::GbmDevice* device) {
  if (!device)
    return {};

  std::vector<gfx::BufferUsageAndFormat> configs;
  for (gfx::BufferUsage usage : {
           gfx::BufferUsage::GPU_READ,
           gfx::BufferUsage::SCANOUT,
           gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
           gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
       }) {
    for (gfx::BufferFormat format : {
             gfx::BufferFormat::R_8,
             gfx::BufferFormat::RG_88,
             gfx::BufferFormat::RGBA_8888,
             gfx::BufferFormat::RGBX_8888,
             gfx::BufferFormat::BGRA_8888,
             gfx::BufferFormat::BGRX_8888,
             gfx::BufferFormat::BGRA_1010102,

             // On some Intel setups calling gbm_bo_create() with this format
             // results in a crash caused by an integer-divide-by-zero.
             // TODO(thomasanderson): Enable this format.
             // gfx::BufferFormat::RGBA_1010102,
             gfx::BufferFormat::BGR_565,
             gfx::BufferFormat::YUV_420_BIPLANAR,
             gfx::BufferFormat::YVU_420,
             gfx::BufferFormat::P010,
         }) {
      // At least on mesa/amdgpu, gbm_device_is_format_supported() lies.  Test
      // format support by creating a buffer directly.  Use a 2x2 buffer so that
      // YUV420 formats get properly tested.
      if (device->CreateBuffer(GetFourCCFormatFromBufferFormat(format),
                               gfx::Size(2, 2), BufferUsageToGbmFlags(usage))) {
        configs.push_back(gfx::BufferUsageAndFormat(usage, format));
      }
    }
  }
  return configs;
}

}  // namespace

// static
GpuMemoryBufferSupportX11* GpuMemoryBufferSupportX11::GetInstance() {
  static base::NoDestructor<GpuMemoryBufferSupportX11> instance;
  return instance.get();
}

GpuMemoryBufferSupportX11::GpuMemoryBufferSupportX11()
    : device_(CreateX11GbmDevice()),
      supported_configs_(CreateSupportedConfigList(device_.get())) {}

GpuMemoryBufferSupportX11::~GpuMemoryBufferSupportX11() = default;

std::unique_ptr<GbmBuffer> GpuMemoryBufferSupportX11::CreateBuffer(
    gfx::BufferFormat format,
    const gfx::Size& size,
    gfx::BufferUsage usage) {
  DCHECK(device_);
  DCHECK(base::Contains(supported_configs_,
                        gfx::BufferUsageAndFormat(usage, format)));

  static base::debug::CrashKeyString* crash_key_string =
      base::debug::AllocateCrashKeyString("buffer_usage_and_format",
                                          base::debug::CrashKeySize::Size64);
  std::string buffer_usage_and_format = gfx::BufferFormatToString(format) +
                                        std::string(",") +
                                        gfx::BufferUsageToString(usage);
  base::debug::ScopedCrashKeyString scoped_crash_key(
      crash_key_string, buffer_usage_and_format.c_str());

  return device_->CreateBuffer(GetFourCCFormatFromBufferFormat(format), size,
                               BufferUsageToGbmFlags(usage));
}

}  // namespace ui
