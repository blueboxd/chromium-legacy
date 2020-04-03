// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_test.h"

#include "base/run_loop.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu_impl.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu_single_process.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host_impl.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host_single_process.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

WaylandTest::WaylandTest()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto keyboard_layout_engine =
      std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_);
#else
  auto keyboard_layout_engine = std::make_unique<StubKeyboardLayoutEngine>();
#endif
  scoped_keyboard_layout_engine_ = std::make_unique<ScopedKeyboardLayoutEngine>(
      std::move(keyboard_layout_engine));
}

WaylandTest::~WaylandTest() {}

void WaylandTest::SetUp() {
  if (use_mojo_) {
    buffer_manager_host_ = std::make_unique<WaylandBufferManagerHostImpl>();
    buffer_manager_gpu_ = std::make_unique<WaylandBufferManagerGpuImpl>();
  } else {
    buffer_manager_host_ =
        std::make_unique<WaylandBufferManagerHostSingleProcess>();
    buffer_manager_gpu_ =
        std::make_unique<WaylandBufferManagerGpuSingleProcess>(
            buffer_manager_host_.get());
    static_cast<WaylandBufferManagerHostSingleProcess*>(
        buffer_manager_host_.get())
        ->SetWaylandBufferManagerGpuSingleProcess(buffer_manager_gpu_.get());
  }

  connection_ = std::make_unique<WaylandConnection>(buffer_manager_host_.get());
  buffer_manager_host_->SetWaylandConnection(connection_.get());
  surface_factory_ = std::make_unique<WaylandSurfaceFactory>(
      connection_.get(), buffer_manager_gpu_.get());

  ASSERT_TRUE(server_.Start(GetParam()));
  ASSERT_TRUE(connection_->Initialize());
  screen_ = connection_->wayland_output_manager()->CreateWaylandScreen(
      connection_.get());
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&widget_));
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = PlatformWindowType::kWindow;
  window_ = WaylandWindow::Create(&delegate_, connection_.get(),
                                  std::move(properties));
  ASSERT_NE(widget_, gfx::kNullAcceleratedWidget);

  window_->Show(false);

  // Wait for the client to flush all pending requests from initialization.
  base::RunLoop().RunUntilIdle();

  // Pause the server after it has responded to all incoming events.
  server_.Pause();

  surface_ = server_.GetObject<wl::MockSurface>(widget_);
  ASSERT_TRUE(surface_);

  initialized_ = true;
}

void WaylandTest::TearDown() {
  if (initialized_)
    Sync();
}

void WaylandTest::Sync() {
  // Resume the server, flushing its pending events.
  server_.Resume();

  // Wait for the client to finish processing these events.
  base::RunLoop().RunUntilIdle();

  // Pause the server, after it has finished processing any follow-up requests
  // from the client.
  server_.Pause();
}

void WaylandTest::SetInitializeWithMojo(bool use_mojo) {
  use_mojo_ = use_mojo;
}

}  // namespace ui
