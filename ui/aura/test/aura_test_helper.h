// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_HELPER_H_
#define UI_AURA_TEST_AURA_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/aura/window_event_dispatcher.h"

namespace ui {
class ContextFactory;
class ScopedAnimationDurationScaleMode;
class TestContextFactories;
}

namespace wm {
class WMState;
}

namespace aura {
class Env;
class TestScreen;
class Window;
class WindowTreeHost;

namespace client {
class CaptureClient;
class DefaultCaptureClient;
class FocusClient;
class ScreenPositionClient;
}
namespace test {
class TestWindowParentingClient;

// A helper class owned by tests that does common initialization required for
// Aura use. This class creates a root window with clients and other objects
// that are necessary to run test on Aura.
class AuraTestHelper {
 public:
  AuraTestHelper();
  explicit AuraTestHelper(std::unique_ptr<Env> env);
  ~AuraTestHelper();

  // Returns the current AuraTestHelper, or nullptr if it's not alive.
  static AuraTestHelper* GetInstance();

  // Creates and initializes (shows and sizes) the RootWindow for use in tests.
  void SetUp(ui::ContextFactory* context_factory = nullptr);

  // Clean up objects that are created for tests. This also deletes the Env
  // object.
  void TearDown();

  // Flushes message loop.
  void RunAllPendingInMessageLoop();

  Window* GetContext();
  WindowTreeHost* GetHost();
  TestScreen* GetTestScreen();
  client::FocusClient* GetFocusClient();
  client::CaptureClient* GetCaptureClient();

  Env* GetEnv();

  // May only be called between SetUp() and TearDown().
  ui::ContextFactory* GetContextFactory();

 private:
  bool setup_called_ = false;
  bool teardown_called_ = false;
  std::unique_ptr<wm::WMState> wm_state_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  std::unique_ptr<Env> env_;
  ui::ContextFactory* context_factory_to_restore_ = nullptr;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<TestScreen> test_screen_;
  std::unique_ptr<WindowTreeHost> host_;
  std::unique_ptr<client::FocusClient> focus_client_;
  std::unique_ptr<client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<TestWindowParentingClient> parenting_client_;
  std::unique_ptr<client::ScreenPositionClient> screen_position_client_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestHelper);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_HELPER_H_
