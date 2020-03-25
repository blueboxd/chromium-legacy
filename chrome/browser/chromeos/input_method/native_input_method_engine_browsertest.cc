// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace {

using input_method::InputMethodEngineBase;

class TestObserver : public InputMethodEngineBase::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override {
    std::move(callback).Run(/*handled=*/false);
  }
  void OnInputContextUpdate(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const base::string16& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class KeyProcessingWaiter {
 public:
  ui::IMEEngineHandlerInterface::KeyEventDoneCallback CreateCallback() {
    return base::BindOnce(&KeyProcessingWaiter::OnKeyEventDone,
                          base::Unretained(this));
  }

  void OnKeyEventDone(bool consumed) { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class NativeInputMethodEngineTest : public InProcessBrowserTest,
                                    public ui::internal::InputMethodDelegate {
 public:
  NativeInputMethodEngineTest() : input_method_(this) {
    feature_list_.InitWithFeatureState(
        chromeos::features::kNativeRuleBasedTyping, true);
  }

 protected:
  void SetUp() override {
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(&input_method_);

    auto observer = std::make_unique<TestObserver>();
    engine_.Initialize(std::move(observer), "", nullptr);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }

  void DispatchKeyPress(ui::KeyboardCode code, int flags = ui::EF_NONE) {
    KeyProcessingWaiter waiterPressed;
    KeyProcessingWaiter waiterReleased;
    engine_.ProcessKeyEvent({ui::ET_KEY_PRESSED, code, flags},
                            waiterPressed.CreateCallback());
    engine_.ProcessKeyEvent({ui::ET_KEY_RELEASED, code, flags},
                            waiterReleased.CreateCallback());
    engine_.FlushForTesting();
    waiterPressed.Wait();
    waiterReleased.Wait();
  }

  void SetFocus(ui::TextInputClient* client) {
    input_method_.SetFocusedTextInputClient(client);
  }

  chromeos::NativeInputMethodEngine engine_;

 private:
  ui::InputMethodChromeOS input_method_;
  base::test::ScopedFeatureList feature_list_;
};

// ID is specified in google_xkb_manifest.json.
constexpr char kEngineIdVietnameseTelex[] = "vkd_vi_telex";
constexpr char kEngineIdArabic[] = "vkd_ar";

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       VietnameseTelex_SimpleTransform) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  DispatchKeyPress(ui::VKEY_S);
  DispatchKeyPress(ui::VKEY_SPACE);

  // Expect to commit 'Á '.
  ASSERT_EQ(text_input_client.composition_history().size(), 2U);
  EXPECT_EQ(text_input_client.composition_history()[0].text,
            base::ASCIIToUTF16("A"));
  EXPECT_EQ(text_input_client.composition_history()[1].text,
            base::UTF8ToUTF16(u8"\u00c1"));
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::UTF8ToUTF16(u8"\u00c1 "));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, VietnameseTelex_Reset) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A);
  engine_.Reset();
  DispatchKeyPress(ui::VKEY_S);

  // Expect to commit 's'.
  ASSERT_EQ(text_input_client.composition_history().size(), 1U);
  EXPECT_EQ(text_input_client.composition_history()[0].text,
            base::ASCIIToUTF16("a"));
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::ASCIIToUTF16("s"));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SwitchActiveController) {
  // Swap between two controllers.
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  engine_.Disable();
  engine_.Enable(kEngineIdArabic);
  engine_.FlushForTesting();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A);

  // Expect to commit 'ش'.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::UTF8ToUTF16(u8"ش"));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, NoActiveController) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  engine_.Disable();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A);
  engine_.Reset();

  // Expect no changes.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 0U);

  SetFocus(nullptr);
}
