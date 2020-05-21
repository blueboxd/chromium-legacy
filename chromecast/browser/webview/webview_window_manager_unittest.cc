// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_window_manager.h"

#include "chromecast/graphics/cast_window_manager_aura.h"
#include "components/exo/surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_type.h"

using ::testing::StrictMock;

namespace chromecast {

class MockCastWindowManager : public CastWindowManagerAura {
 public:
  MOCK_METHOD(void, SetEnableRoundedCorners, (bool enable), (override));

 private:
  using CastWindowManagerAura::CastWindowManagerAura;
};

class WebviewWindowManagerTest : public testing::Test {
 public:
  WebviewWindowManagerTest()
      : env_(aura::Env::CreateInstance()),
        mock_cast_window_manager_(new StrictMock<MockCastWindowManager>(true)),
        webview_window_manager_(
            new WebviewWindowManager(mock_cast_window_manager_.get())) {}

  WebviewWindowManagerTest(const WebviewWindowManagerTest&) = delete;
  WebviewWindowManagerTest& operator=(const WebviewWindowManagerTest&) = delete;

 protected:
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<StrictMock<MockCastWindowManager>> mock_cast_window_manager_;
  std::unique_ptr<WebviewWindowManager> webview_window_manager_;
};

TEST_F(WebviewWindowManagerTest, NoSetProperty) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();
  window->Hide();
  // StrictMock is used to verify SetEnableRoundedCorners is not called when
  // kClientSurfaceIdKey property is not set.
}

TEST_F(WebviewWindowManagerTest,
       SetRoundedCornersOnWindowAfterSettingExoPropertyAndShowing) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetProperty(exo::kClientSurfaceIdKey, 1);
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window = nullptr;
}

TEST_F(WebviewWindowManagerTest,
       SetRoundedCornersOnVisibleWindowAfterSettingExoProperty) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->SetProperty(exo::kClientSurfaceIdKey, 1);

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window = nullptr;
}

TEST_F(WebviewWindowManagerTest, RemoveRoundedCornersAfterHidingWindow) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetProperty(exo::kClientSurfaceIdKey, 1);
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window->Hide();
}

TEST_F(WebviewWindowManagerTest,
       RemoveRoundedCornersAfterHidingMultipleWindows) {
  std::unique_ptr<aura::Window> window1 =
      std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_TEXTURED);
  std::unique_ptr<aura::Window> window2 =
      std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_TEXTURED);
  window1->SetProperty(exo::kClientSurfaceIdKey, 1);
  window2->SetProperty(exo::kClientSurfaceIdKey, 2);
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true))
      .Times(3);
  window1->Show();
  window2->Show();
  window1->Hide();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window2->Hide();
}

TEST_F(WebviewWindowManagerTest,
       RemoveRoundedCornersAfterDestroyingMultipleWindows) {
  std::unique_ptr<aura::Window> window1 =
      std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_TEXTURED);
  std::unique_ptr<aura::Window> window2 =
      std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_TEXTURED);
  window1->SetProperty(exo::kClientSurfaceIdKey, 1);
  window2->SetProperty(exo::kClientSurfaceIdKey, 2);
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true))
      .Times(2);
  window1->Show();
  window2->Show();
  window1 = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window2 = nullptr;
}

}  // namespace chromecast
