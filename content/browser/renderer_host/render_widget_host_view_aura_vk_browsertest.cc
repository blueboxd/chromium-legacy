// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/input_method_keyboard_controller_observer.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class MockKeyboardController : public ui::InputMethodKeyboardController {
 public:
  bool DisplayVirtualKeyboard() override { return true; }
  void DismissVirtualKeyboard() override {}
  void AddObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(
      ui::InputMethodKeyboardControllerObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOnKeyboardShown(gfx::Rect dip_rect) {
    is_keyboard_visible_ = true;
    for (ui::InputMethodKeyboardControllerObserver& observer : observers_)
      observer.OnKeyboardVisible(dip_rect);
  }

  void NotifyObserversOnKeyboardHidden() {
    is_keyboard_visible_ = false;
    for (ui::InputMethodKeyboardControllerObserver& observer : observers_)
      observer.OnKeyboardHidden();
  }

  bool IsKeyboardVisible() override { return is_keyboard_visible_; }

 private:
  base::ObserverList<ui::InputMethodKeyboardControllerObserver,
                     false>::Unchecked observers_;
  bool is_keyboard_visible_ = false;
};

class KeyboardControllerMockInputMethod : public ui::MockInputMethod {
 public:
  KeyboardControllerMockInputMethod() : ui::MockInputMethod(nullptr) {}
  ui::InputMethodKeyboardController* GetInputMethodKeyboardController()
      override {
    return &mock_keyboard_controller_;
  }

  MockKeyboardController* GetMockKeyboardController() {
    return &mock_keyboard_controller_;
  }
  MockKeyboardController mock_keyboard_controller_;
};

class RenderWidgetHostViewAuraBrowserMockIMETest : public ContentBrowserTest {
 public:
  void SetUp() override {
    input_method_ = new KeyboardControllerMockInputMethod;
    // transfers ownership.
    ui::SetUpInputMethodForTesting(input_method_);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "VirtualKeyboard");
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostViewAura* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewAura*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

 protected:
  KeyboardControllerMockInputMethod* input_method_ = nullptr;
};

#ifdef OS_WIN
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewAuraBrowserMockIMETest,
                       VirtualKeyboardIntegrationTest) {
  // The keyboard input pane events are not supported on Win7.
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    return;
  }
  const char kVirtualKeyboardDataURL[] =
      "data:text/html,<!DOCTYPE html>"
      "<script>"
      "  let x, y, width, height, numEvents = 0;"
      "  navigator.virtualKeyboard.overlaysContent = true;"
      "  navigator.virtualKeyboard.addEventListener('overlaygeometrychange',"
      "   evt => {"
      "     numEvents++;"
      "     let r = evt.boundingRect;"
      "     x = r.x; y = r.y; width = r.width; height = r.height;"
      "   }, false);"
      "</script>";
  EXPECT_TRUE(NavigateToURL(shell(), GURL(kVirtualKeyboardDataURL)));

  RenderWidgetHostViewAura* rwhvi = GetRenderWidgetHostView();

  // Send a touch event so that RenderWidgetHostViewAura will create the
  // keyboard observer (requires last_pointer_type_ to be TOUCH).
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(30, 30),
                       base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhvi->OnTouchEvent(&press);

  // Emulate input type text focus in the root frame (the only frame), by
  // setting frame focus and updating TextInputState. This is a more direct way
  // of triggering focus in a textarea in the web page.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents->GetFrameTree()->root();
  web_contents->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  TextInputState text_input_state;
  text_input_state.show_ime_if_needed = true;
  text_input_state.type = ui::TEXT_INPUT_TYPE_TEXT;

  TextInputManager* text_input_manager = rwhvi->GetTextInputManager();
  text_input_manager->UpdateTextInputState(rwhvi, text_input_state);

  // Send through a keyboard showing event with a rect, and verify the
  // javascript event fires with the appropriate values.
  constexpr int kKeyboardX = 0;
  constexpr int kKeyboardY = 200;
  constexpr int kKeyboardWidth = 200;
  constexpr int kKeyboardHeight = 200;
  gfx::Rect keyboard_rect(kKeyboardX, kKeyboardY, kKeyboardWidth,
                          kKeyboardHeight);
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);

  // There are x and y-offsets for the main frame in content_browsertest
  // hosting. We need to take these into account for the expected values.
  gfx::PointF root_widget_origin(0.f, 0.f);
  rwhvi->TransformPointToRootSurface(&root_widget_origin);
  const int expected_width = kKeyboardWidth - root_widget_origin.x();
  const int expected_y = kKeyboardY - root_widget_origin.y();

  EXPECT_EQ(1, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(0, EvalJs(shell(), "x"));
  EXPECT_EQ(expected_y, EvalJs(shell(), "y"));
  EXPECT_EQ(expected_width, EvalJs(shell(), "width"));
  EXPECT_EQ(kKeyboardHeight, EvalJs(shell(), "height"));

  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardHidden();
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
  EXPECT_EQ(0, EvalJs(shell(), "width"));
  EXPECT_EQ(0, EvalJs(shell(), "height"));
  EXPECT_EQ(0, EvalJs(shell(), "x"));
  EXPECT_EQ(0, EvalJs(shell(), "y"));

  // Flip the policy back to non-overlay, verify the event doesn't fire
  ignore_result(
      EvalJs(shell(), "navigator.virtualKeyboard.overlaysContent = false"));
  input_method_->GetMockKeyboardController()->NotifyObserversOnKeyboardShown(
      keyboard_rect);
  EXPECT_EQ(2, EvalJs(shell(), "numEvents"));
}
#endif  // #ifdef OS_WIN

}  // namespace content
