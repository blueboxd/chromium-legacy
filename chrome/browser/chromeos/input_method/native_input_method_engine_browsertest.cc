// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/chromeos/input_method/ime_service_connector.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/aura/window_tree_host.h"
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

class TestPersonalDataManagerObserver
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit TestPersonalDataManagerObserver(Profile* profile)
      : profile_(profile) {
    autofill::PersonalDataManagerFactory::GetForProfile(profile_)->AddObserver(
        this);
  }
  ~TestPersonalDataManagerObserver() override {}

  // Waits for the PersonalDataManager's list of profiles to be updated.
  void Wait() {
    run_loop_.Run();
    autofill::PersonalDataManagerFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override { run_loop_.Quit(); }

 private:
  Profile* profile_;
  base::RunLoop run_loop_;
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
    feature_list_.InitWithFeatures({chromeos::features::kNativeRuleBasedTyping,
                                    chromeos::features::kAssistPersonalInfo},
                                   {});
  }

 protected:
  void SetUp() override {
    chromeos::input_method::DisableImeSandboxForTesting();
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(&input_method_);
    ui::IMEBridge::Get()->SetCurrentEngineHandler(&engine_);

    auto observer = std::make_unique<TestObserver>();

    profile_ = browser()->profile();
    engine_.Initialize(std::move(observer), "", profile_);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpTextInput(chromeos::TextInputTestHelper& helper) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("textinput")),
        base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_TRUE(content::ExecuteScript(
        tab, "document.getElementById('text_id').focus()"));
    helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);

    SetFocus(helper.GetTextInputClient());
  }

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }

  void DispatchKeyPress(ui::KeyboardCode code,
                        bool need_flush,
                        int flags = ui::EF_NONE) {
    KeyProcessingWaiter waiterPressed;
    KeyProcessingWaiter waiterReleased;
    engine_.ProcessKeyEvent({ui::ET_KEY_PRESSED, code, flags},
                            waiterPressed.CreateCallback());
    engine_.ProcessKeyEvent({ui::ET_KEY_RELEASED, code, flags},
                            waiterReleased.CreateCallback());
    if (need_flush)
      engine_.FlushForTesting();

    waiterPressed.Wait();
    waiterReleased.Wait();
  }

  void SetFocus(ui::TextInputClient* client) {
    input_method_.SetFocusedTextInputClient(client);
  }

  ui::InputMethod* GetBrowserInputMethod() {
    return browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  }

  chromeos::NativeInputMethodEngine engine_;
  Profile* profile_;

 private:
  ui::InputMethodChromeOS input_method_;
  base::test::ScopedFeatureList feature_list_;
};

// ID is specified in google_xkb_manifest.json.
constexpr char kEngineIdArabic[] = "vkd_ar";
constexpr char kEngineIdUs[] = "xkb:us::eng";
constexpr char kEngineIdVietnameseTelex[] = "vkd_vi_telex";

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       VietnameseTelex_SimpleTransform) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true, ui::EF_SHIFT_DOWN);
  DispatchKeyPress(ui::VKEY_S, true);
  DispatchKeyPress(ui::VKEY_SPACE, true);

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

  DispatchKeyPress(ui::VKEY_A, true);
  engine_.Reset();
  DispatchKeyPress(ui::VKEY_S, true);

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

  DispatchKeyPress(ui::VKEY_A, true);

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

  DispatchKeyPress(ui::VKEY_A, true);
  engine_.Reset();

  // Expect no changes.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 0U);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestUserEmail) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile_);
  signin::SetPrimaryAccount(identity_manager, "johnwayne@me.xyz");

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my email is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my email is johnwayne@me.xyz");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);

  DispatchKeyPress(ui::VKEY_TAB, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, DismissSuggestion) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile_);
  signin::SetPrimaryAccount(identity_manager, "johnwayne@me.xyz");

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my email is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my email is john@abc.com");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);

  DispatchKeyPress(ui::VKEY_ESCAPE, false);
  // This tab should make no effect.
  DispatchKeyPress(ui::VKEY_TAB, false);
  helper.GetTextInputClient()->InsertText(base::UTF8ToUTF16("john@abc.com"));
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestUserName) {
  TestPersonalDataManagerObserver personal_data_observer(profile_);
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                              base::UTF8ToUTF16("John Wayne"));
  autofill::PersonalDataManagerFactory::GetForProfile(profile_)->AddProfile(
      autofill_profile);
  personal_data_observer.Wait();

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my name is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my name is John Wayne");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);

  // Keep typing
  helper.GetTextInputClient()->InsertText(base::UTF8ToUTF16("jo"));
  helper.WaitForSurroundingTextChanged(base::UTF8ToUTF16("my name is jo"));

  DispatchKeyPress(ui::VKEY_TAB, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());

  SetFocus(nullptr);
}
