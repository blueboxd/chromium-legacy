// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

const int kTextAreaHeight = 36;
const int kTextAreaWidth = 162;
const int kTextAreaOffsetX = 100;

// TextInputManager Observers

// A base class for observing the TextInputManager owned by the given
// WebContents. Subclasses could observe the TextInputManager for different
// changes. The class wraps a public tester which accepts callbacks that
// are run after specific changes in TextInputManager. Different observers can
// be subclassed from this by providing their specific callback methods.
class TextInputManagerObserverBase {
 public:
  explicit TextInputManagerObserverBase(content::WebContents* web_contents)
      : tester_(new content::TextInputManagerTester(web_contents)),
        success_(false) {}

  virtual ~TextInputManagerObserverBase() {}

  // Wait for derived class's definition of success.
  void Wait() {
    if (success_)
      return;
    run_loop.Run();
  }

  bool success() const { return success_; }

 protected:
  content::TextInputManagerTester* tester() { return tester_.get(); }

  void OnSuccess() {
    success_ = true;
    run_loop.Quit();

    // By deleting |tester_| we make sure that the internal observer used in
    // content/ is removed from the observer list of TextInputManager.
    tester_.reset(nullptr);
  }

 private:
  std::unique_ptr<content::TextInputManagerTester> tester_;
  bool success_;
  base::RunLoop run_loop;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerObserverBase);
};

// This class observes TextInputManager for changes in
// |TextInputState.vk_policy|.
class TextInputManagerVkPolicyObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerVkPolicyObserver(
      content::WebContents* web_contents,
      ui::mojom::VirtualKeyboardPolicy expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(
        base::BindRepeating(&TextInputManagerVkPolicyObserver::VerifyVkPolicy,
                            base::Unretained(this)));
  }

 private:
  void VerifyVkPolicy() {
    ui::mojom::VirtualKeyboardPolicy value;
    if (tester()->GetTextInputVkPolicy(&value) && expected_value_ == value)
      OnSuccess();
  }

  ui::mojom::VirtualKeyboardPolicy expected_value_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerVkPolicyObserver);
};

// This class observes TextInputManager for changes in
// |TextInputState.last_vk_visibility_request|.
class TextInputManagerVkVisibilityRequestObserver
    : public TextInputManagerObserverBase {
 public:
  TextInputManagerVkVisibilityRequestObserver(
      content::WebContents* web_contents,
      ui::mojom::VirtualKeyboardVisibilityRequest expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerVkVisibilityRequestObserver::VerifyVkVisibilityRequest,
        base::Unretained(this)));
  }

 private:
  void VerifyVkVisibilityRequest() {
    ui::mojom::VirtualKeyboardVisibilityRequest value;
    if (tester()->GetTextInputVkVisibilityRequest(&value) &&
        expected_value_ == value)
      OnSuccess();
  }

  ui::mojom::VirtualKeyboardVisibilityRequest expected_value_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerVkVisibilityRequestObserver);
};

// This class observes TextInputManager for changes in
// |TextInputState.show_ime_if_needed|.
class TextInputManagerShowImeIfNeededObserver
    : public TextInputManagerObserverBase {
 public:
  TextInputManagerShowImeIfNeededObserver(content::WebContents* web_contents,
                                          bool expected_value)
      : TextInputManagerObserverBase(web_contents),
        expected_value_(expected_value) {
    tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
        &TextInputManagerShowImeIfNeededObserver::VerifyShowImeIfNeeded,
        base::Unretained(this)));
  }

 private:
  void VerifyShowImeIfNeeded() {
    bool show_ime_if_needed;
    if (tester()->GetTextInputShowImeIfNeeded(&show_ime_if_needed) &&
        expected_value_ == show_ime_if_needed)
      OnSuccess();
  }

  bool expected_value_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManagerShowImeIfNeededObserver);
};

class VirtualKeyboardPolicyTest : public InProcessBrowserTest {
 public:
  VirtualKeyboardPolicyTest() {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  content::WebContents* GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ui::BaseWindow* GetWindow() const { return browser()->window(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "VirtualKeyboard,EditContext");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // Wait for the active web contents title to match |title|.
  void WaitForTitle(const std::string& title) {
    const base::string16 expected_title(base::ASCIIToUTF16(title));
    content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void NavigateAndWaitForLoad() {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    // Navigate to the test page and wait for onload to be called.
    const GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(),
        base::FilePath(FILE_PATH_LITERAL("virtual_keyboard_policy.html")));
    ui_test_utils::NavigateToURL(browser(), url);
    WaitForTitle("onload");
  }

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardPolicyTest);
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, Load) {
  NavigateAndWaitForLoad();
}

// Tapping on an editable element should show VK.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, ShowVK) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }

  NavigateAndWaitForLoad();
  // Tap on the third textarea to open VK.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkPolicyObserver type_observer_auto(
      GetActiveWebContents(), ui::mojom::VirtualKeyboardPolicy::AUTO);
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1,
      bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX * 2,
      bounds.y() + kTextAreaHeight / 2));
  type_observer_auto.Wait();
}

// Tapping on an editable element with virtualkeyboardpolicy="auto" should
// raise the VK but JS focus shouldn't.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, DontShowVKOnJSFocus) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }

  NavigateAndWaitForLoad();
  content::RenderFrameHost* current = GetActiveWebContents()->GetMainFrame();
  TextInputManagerShowImeIfNeededObserver show_ime_observer_false(
      GetActiveWebContents(), false);
  // Run the JS that focuses the edit control.
  std::string script =
      "inputField = document.getElementById('txt4');"
      "inputField.focus();";
  EXPECT_TRUE(ExecuteScriptWithoutUserGesture(current, script));
  show_ime_observer_false.Wait();
  TextInputManagerShowImeIfNeededObserver show_ime_observer_true(
      GetActiveWebContents(), true);
  // Tap on the third textarea to open VK.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1,
      bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX * 2,
      bounds.y() + kTextAreaHeight / 2));
  show_ime_observer_true.Wait();
}

// Tapping on an editable element with virtualkeyboardpolicy="manual" that
// calls hide() explicitly should hide VK.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, HideVK) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }
  NavigateAndWaitForLoad();

  // Tap on the first textarea that would trigger show() call.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1, bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX,
      bounds.y() + kTextAreaHeight / 2));
  type_observer_hide.Wait();
}

// Tapping on an editable element with virtualkeyboardpolicy="manual" that
// calls show() explicitly should show the VK and if hide()is called, then it
// should hide VK.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, ShowAndThenHideVK) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }
  NavigateAndWaitForLoad();

  // Tap on the first textarea that would trigger show() call and then on the
  // second textarea that would trigger hide() call.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::PRESS, 1,
                                           bounds.x() + kTextAreaWidth / 2,
                                           bounds.y() + kTextAreaHeight / 2));
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1, bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX,
      bounds.y() + kTextAreaHeight / 2));
  type_observer_hide.Wait();
}

// Tapping on an editable element with virtualkeyboardpolicy="manual" that
// calls show() explicitly should show the VK and if hide()is called, then it
// should hide VK on keydown.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest, ShowAndThenHideVKOnKeyDown) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }
  NavigateAndWaitForLoad();

  // Tap on the first textarea that would trigger show() call and then on the
  // second textarea that would trigger hide() call.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::PRESS, 1,
                                           bounds.x() + kTextAreaWidth / 2,
                                           bounds.y() + kTextAreaHeight / 2));
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  ASSERT_TRUE(ui_controls::SendKeyPress(GetWindow()->GetNativeWindow(),
                                        ui::VKEY_RETURN, false, false, false,
                                        false));
  type_observer_hide.Wait();
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest,
                       VKVisibilityRequestInDeletedDocument) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }
  NavigateAndWaitForLoad();

  // Tap on the first textarea that would trigger show() call and then on the
  // second textarea that would trigger hide() call.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkVisibilityRequestObserver type_observer_none(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::NONE);
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1,
      bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX * 8,
      bounds.y() + kTextAreaHeight / 2));
  type_observer_none.Wait();
}

// Tapping on an editcontext with inputpanelpolicy="manual" that
// calls show() explicitly should show the VK and if hide() is called, then it
// should hide VK.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardPolicyTest,
                       ShowAndThenHideVKInEditContext) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    return;
  }
  NavigateAndWaitForLoad();

  // Tap on the textarea that would trigger show() call and then on the
  // second textarea that would trigger hide() call.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  TextInputManagerVkVisibilityRequestObserver type_observer_show(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::SHOW);
  ASSERT_TRUE(ui_controls::SendTouchEvents(
      ui_controls::PRESS, 1,
      bounds.x() + kTextAreaWidth / 2 + kTextAreaOffsetX * 4,
      bounds.y() + kTextAreaHeight / 2));
  type_observer_show.Wait();
  TextInputManagerVkVisibilityRequestObserver type_observer_hide(
      GetActiveWebContents(),
      ui::mojom::VirtualKeyboardVisibilityRequest::HIDE);
  ASSERT_TRUE(ui_controls::SendKeyPress(GetWindow()->GetNativeWindow(),
                                        ui::VKEY_RETURN, false, false, false,
                                        false));
  type_observer_hide.Wait();
}

}  // namespace
