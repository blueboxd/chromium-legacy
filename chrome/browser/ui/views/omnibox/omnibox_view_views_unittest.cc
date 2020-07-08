// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#endif

using gfx::Range;
using metrics::OmniboxEventProto;

namespace {

// TestingOmniboxView ---------------------------------------------------------

class TestingOmniboxView : public OmniboxViewViews {
 public:
  TestingOmniboxView(OmniboxEditController* controller,
                     std::unique_ptr<OmniboxClient> client);

  using views::Textfield::GetRenderText;

  void ResetEmphasisTestState();

  void CheckUpdatePopupCallInfo(size_t call_count,
                                const base::string16& text,
                                const Range& selection_range);

  void CheckUpdatePopupNotCalled();

  Range scheme_range() const { return scheme_range_; }
  Range emphasis_range() const { return emphasis_range_; }
  bool base_text_emphasis() const { return base_text_emphasis_; }

  // OmniboxViewViews:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {}
  void OnThemeChanged() override;

  using OmniboxView::OnInlineAutocompleteTextMaybeChanged;

 private:
  // OmniboxViewViews:
  // There is no popup and it doesn't actually matter whether we change the
  // visual style of the text, so these methods are all overridden merely to
  // capture relevant state at the time of the call, to be checked by test code.
  void UpdatePopup() override;
  void SetEmphasis(bool emphasize, const Range& range) override;
  void UpdateSchemeStyle(const Range& range) override;

  size_t update_popup_call_count_ = 0;
  base::string16 update_popup_text_;
  Range update_popup_selection_range_;

  // Range of the last scheme logged by UpdateSchemeStyle().
  Range scheme_range_;

  // Range of the last text emphasized by SetEmphasis().
  Range emphasis_range_;

  // SetEmphasis() logs whether the base color of the text is emphasized.
  bool base_text_emphasis_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

TestingOmniboxView::TestingOmniboxView(OmniboxEditController* controller,
                                       std::unique_ptr<OmniboxClient> client)
    : OmniboxViewViews(controller,
                       std::move(client),
                       false,
                       nullptr,
                       gfx::FontList()) {}

void TestingOmniboxView::ResetEmphasisTestState() {
  base_text_emphasis_ = false;
  emphasis_range_ = Range::InvalidRange();
  scheme_range_ = Range::InvalidRange();
}

void TestingOmniboxView::CheckUpdatePopupCallInfo(
    size_t call_count,
    const base::string16& text,
    const Range& selection_range) {
  EXPECT_EQ(call_count, update_popup_call_count_);
  EXPECT_EQ(text, update_popup_text_);
  EXPECT_EQ(selection_range, update_popup_selection_range_);
}

void TestingOmniboxView::CheckUpdatePopupNotCalled() {
  EXPECT_EQ(update_popup_call_count_, 0U);
}

void TestingOmniboxView::OnThemeChanged() {
  // This method is overridden simply to expose this protected method for tests
  // to call.
  OmniboxViewViews::OnThemeChanged();
}

void TestingOmniboxView::UpdatePopup() {
  ++update_popup_call_count_;
  update_popup_text_ = GetText();
  update_popup_selection_range_ = GetSelectedRange();

  // The real view calls OmniboxEditModel::UpdateInput(), which sets input in
  // progress and starts autocomplete.  Triggering autocomplete and the popup is
  // beyond the scope of this test, but setting input in progress is important
  // for making some sequences (e.g. uneliding on taking an action) behave
  // correctly.
  model()->SetInputInProgress(true);
}

void TestingOmniboxView::SetEmphasis(bool emphasize, const Range& range) {
  if (range == Range::InvalidRange()) {
    base_text_emphasis_ = emphasize;
    return;
  }

  EXPECT_TRUE(emphasize);
  emphasis_range_ = range;
}

void TestingOmniboxView::UpdateSchemeStyle(const Range& range) {
  scheme_range_ = range;
}

// TestingOmniboxEditController -----------------------------------------------

class TestingOmniboxEditController : public ChromeOmniboxEditController {
 public:
  TestingOmniboxEditController(CommandUpdater* command_updater,
                               LocationBarModel* location_bar_model)
      : ChromeOmniboxEditController(command_updater),
        location_bar_model_(location_bar_model) {}

  void set_omnibox_view(OmniboxViewViews* view) { omnibox_view_ = view; }

 private:
  // ChromeOmniboxEditController:
  LocationBarModel* GetLocationBarModel() override {
    return location_bar_model_;
  }
  const LocationBarModel* GetLocationBarModel() const override {
    return location_bar_model_;
  }
  void UpdateWithoutTabRestore() override {
    // This is a minimal amount of what LocationBarView does. Not all tests
    // set |omnibox_view_|.
    if (omnibox_view_)
      omnibox_view_->Update();
  }

  LocationBarModel* location_bar_model_;
  OmniboxViewViews* omnibox_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

}  // namespace

// OmniboxViewViewsTest -------------------------------------------------------

// Base class that ensures ScopedFeatureList is initialized first.
class OmniboxViewViewsTestBase : public ChromeViewsTestBase {
 public:
  OmniboxViewViewsTestBase(
      const std::vector<base::Feature>& enabled_features,
      const std::vector<base::Feature>& disabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The display URL used in simplified domain display tests.
const base::string16 kSimplifiedDomainDisplayUrl =
    base::ASCIIToUTF16("foo.example.test/bar");

class OmniboxViewViewsTest : public OmniboxViewViewsTestBase {
 public:
  OmniboxViewViewsTest(const std::vector<base::Feature>& enabled_features,
                       const std::vector<base::Feature>& disabled_features);

  OmniboxViewViewsTest()
      : OmniboxViewViewsTest(std::vector<base::Feature>(),
                             std::vector<base::Feature>()) {}

  TestLocationBarModel* location_bar_model() { return &location_bar_model_; }
  CommandUpdaterImpl* command_updater() { return &command_updater_; }
  TestingOmniboxView* omnibox_view() const { return omnibox_view_; }

  // TODO(tommycli): These base class accessors exist because Textfield and
  // OmniboxView both hide member functions that were public in base classes.
  // Remove these after we stop doing that.
  views::Textfield* omnibox_textfield() const { return omnibox_view(); }
  views::View* omnibox_textfield_view() const { return omnibox_view(); }

  views::TextfieldTestApi* textfield_test_api() { return test_api_.get(); }

  // Sets |new_text| as the omnibox text, and emphasizes it appropriately.  If
  // |accept_input| is true, pretends that the user has accepted this input
  // (i.e. it's been navigated to).
  void SetAndEmphasizeText(const std::string& new_text, bool accept_input);

  bool IsCursorEnabled() const {
    return test_api_->GetRenderText()->cursor_enabled();
  }

  ui::MouseEvent CreateMouseEvent(ui::EventType type,
                                  const gfx::Point& point,
                                  int event_flags = ui::EF_LEFT_MOUSE_BUTTON) {
    return ui::MouseEvent(type, point, point, ui::EventTimeForNow(),
                          event_flags, event_flags);
  }

 protected:
  Profile* profile() { return profile_.get(); }
  TestingOmniboxEditController* omnibox_edit_controller() {
    return &omnibox_edit_controller_;
  }

  // Sets up tests for the simplified domain field trials.
  void SetUpSimplifiedDomainTest() {
    location_bar_model()->set_url(
        GURL(base::ASCIIToUTF16("https://") + kSimplifiedDomainDisplayUrl));
    location_bar_model()->set_url_for_display(kSimplifiedDomainDisplayUrl);
    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
    // Call OnThemeChanged() to create the animations.
    omnibox_view()->OnThemeChanged();
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::MouseEvent CreateEvent(ui::EventType type, int flags) {
    return ui::MouseEvent(type, gfx::Point(0, 0), gfx::Point(),
                          ui::EventTimeForNow(), flags, 0);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TemplateURLServiceFactoryTestUtil> util_;
  CommandUpdaterImpl command_updater_;
  TestLocationBarModel location_bar_model_;
  TestingOmniboxEditController omnibox_edit_controller_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<views::Widget> widget_;

  // Owned by |widget_|.
  TestingOmniboxView* omnibox_view_;

  std::unique_ptr<views::TextfieldTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViewsTest);
};

OmniboxViewViewsTest::OmniboxViewViewsTest(
    const std::vector<base::Feature>& enabled_features,
    const std::vector<base::Feature>& disabled_features)
    : OmniboxViewViewsTestBase(enabled_features, disabled_features),
      command_updater_(nullptr),
      omnibox_edit_controller_(&command_updater_, &location_bar_model_) {}

void OmniboxViewViewsTest::SetAndEmphasizeText(const std::string& new_text,
                                               bool accept_input) {
  omnibox_view()->ResetEmphasisTestState();
  omnibox_view()->SetUserText(base::ASCIIToUTF16(new_text));
  if (accept_input) {
    // We don't need to actually navigate in this case (and doing so in a test
    // would be difficult); it's sufficient to mark input as "no longer in
    // progress", and the edit model will assume the current text is a URL.
    omnibox_view()->model()->SetInputInProgress(false);
  }
  omnibox_view()->EmphasizeURLComponents();
}

void OmniboxViewViewsTest::SetUp() {
  ChromeViewsTestBase::SetUp();

  profile_ = std::make_unique<TestingProfile>();
  util_ = std::make_unique<TemplateURLServiceFactoryTestUtil>(profile_.get());

  // We need a widget so OmniboxView can be correctly focused and unfocused.
  widget_ = CreateTestWidget();
  widget_->Show();

#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManagerImpl);
#endif
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));
  auto omnibox_view = std::make_unique<TestingOmniboxView>(
      &omnibox_edit_controller_,
      std::make_unique<ChromeOmniboxClient>(&omnibox_edit_controller_,
                                            profile_.get()));
  test_api_ = std::make_unique<views::TextfieldTestApi>(omnibox_view.get());
  omnibox_view->Init();

  omnibox_view_ = widget_->SetContentsView(std::move(omnibox_view));
}

void OmniboxViewViewsTest::TearDown() {
  // Clean ourselves up as the text input client.
  if (omnibox_view_->GetInputMethod())
    omnibox_view_->GetInputMethod()->DetachTextInputClient(omnibox_view_);

  widget_.reset();
  util_.reset();
  profile_.reset();

#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  ChromeViewsTestBase::TearDown();
}

// Actual tests ---------------------------------------------------------------

// Checks that a single change of the text in the omnibox invokes
// only one call to OmniboxViewViews::UpdatePopup().
TEST_F(OmniboxViewViewsTest, UpdatePopupCall) {
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(1, base::ASCIIToUTF16("a"),
                                           Range(1));

  char_event =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0,
                   ui::DomKey::FromCharacter('b'), ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  omnibox_view()->CheckUpdatePopupCallInfo(2, base::ASCIIToUTF16("ab"),
                                           Range(2));

  ui::KeyEvent pressed(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&pressed);
  omnibox_view()->CheckUpdatePopupCallInfo(3, base::ASCIIToUTF16("a"),
                                           Range(1));
}

// Test that text cursor is shown in the omnibox after entering any single
// character in NTP 'Search box'. Test for crbug.com/698172.
TEST_F(OmniboxViewViewsTest, EditTextfield) {
  omnibox_textfield()->SetCursorEnabled(false);
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  EXPECT_TRUE(IsCursorEnabled());
}

// Test that the scheduled text edit command is cleared when Textfield receives
// a key press event. This ensures that the scheduled text edit command property
// is always in the correct state. Test for http://crbug.com/613948.
TEST_F(OmniboxViewViewsTest, ScheduledTextEditCommand) {
  omnibox_textfield()->SetTextEditCommandForNextKeyEvent(
      ui::TextEditCommand::MOVE_UP);
  EXPECT_EQ(ui::TextEditCommand::MOVE_UP,
            textfield_test_api()->scheduled_text_edit_command());

  ui::KeyEvent up_pressed(ui::ET_KEY_PRESSED, ui::VKEY_UP, 0);
  omnibox_textfield()->OnKeyEvent(&up_pressed);
  EXPECT_EQ(ui::TextEditCommand::INVALID_COMMAND,
            textfield_test_api()->scheduled_text_edit_command());
}

// Test that Shift+Up and Shift+Down are not captured and let selection mode
// take over. Test for crbug.com/863543 and crbug.com/892216.
TEST_F(OmniboxViewViewsTest, SelectWithShift_863543) {
  location_bar_model()->set_url(GURL("http://www.example.com/?query=1"));
  const base::string16 text =
      base::ASCIIToUTF16("http://www.example.com/?query=1");
  omnibox_view()->SetWindowTextAndCaretPos(text, 23U, false, false);

  ui::KeyEvent shift_up_pressed(ui::ET_KEY_PRESSED, ui::VKEY_UP,
                                ui::EF_SHIFT_DOWN);
  omnibox_textfield()->OnKeyEvent(&shift_up_pressed);

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(23U, start);
  EXPECT_EQ(0U, end);
  omnibox_view()->CheckUpdatePopupNotCalled();

  omnibox_view()->SetWindowTextAndCaretPos(text, 18U, false, false);

  ui::KeyEvent shift_down_pressed(ui::ET_KEY_PRESSED, ui::VKEY_DOWN,
                                  ui::EF_SHIFT_DOWN);
  omnibox_textfield()->OnKeyEvent(&shift_down_pressed);

  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(18U, start);
  EXPECT_EQ(31U, end);
  omnibox_view()->CheckUpdatePopupNotCalled();
}

TEST_F(OmniboxViewViewsTest, OnBlur) {
  // Make the Omnibox very narrow (so it couldn't fit the whole string).
  int kOmniboxWidth = 60;
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(0, 0, kOmniboxWidth, 10));
  render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // (In this example, uppercase Latin letters represent Hebrew letters.)
  // The string |kContentsRtl| is equivalent to:
  //     RA.QWM/0123/abcd
  // This is displayed as:
  //     0123/MWQ.AR/abcd
  // Enter focused mode, where the text should *not* be elided, and we expect
  // SetWindowTextAndCaretPos to scroll such that the start of the string is
  // on-screen. Because the domain is RTL, this scrolls to an offset greater
  // than 0.
  omnibox_textfield()->OnFocus();
  const base::string16 kContentsRtl =
      base::WideToUTF16(L"\x05e8\x05e2.\x05e7\x05d5\x05dd/0123/abcd");
  omnibox_view()->SetWindowTextAndCaretPos(kContentsRtl, 0, false, false);
  EXPECT_EQ(gfx::NO_ELIDE, render_text->elide_behavior());

  // TODO(https://crbug.com/1094386): this assertion fails because
  // EmphasizeURLComponents() sets the textfield's directionality to
  // DIRECTIONALITY_AS_URL. This should be either fixed or the assertion
  // removed.
  //
  // NOTE: Technically (depending on the font), this expectation could fail if
  // the entire domain fits in 60 pixels. However, 60px is so small it should
  // never happen with any font.
  // EXPECT_GT(0, render_text->GetUpdatedDisplayOffset().x());

  omnibox_view()->SelectAll(false);
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Now enter blurred mode, where the text should be elided to 60px. This means
  // the string itself is truncated. Scrolling would therefore mean the text is
  // off-screen. Ensure that the horizontal scrolling has been reset to 0.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(gfx::ELIDE_TAIL, render_text->elide_behavior());
  EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, Emphasis) {
  constexpr struct {
    const char* input;
    bool expected_base_text_emphasized;
    Range expected_emphasis_range;
    Range expected_scheme_range;
  } test_cases[] = {
      {"data:text/html,Hello%20World", false, Range(0, 4), Range(0, 4)},
      {"http://www.example.com/path/file.htm", false, Range(7, 22),
       Range(0, 4)},
      {"https://www.example.com/path/file.htm", false, Range(8, 23),
       Range(0, 5)},
      {"chrome-extension://ldfbacdbackkjhclmhnjabngnppnkagl", false,
       Range::InvalidRange(), Range(0, 16)},
      {"nosuchscheme://opaque/string", true, Range::InvalidRange(),
       Range(0, 12)},
      {"nosuchscheme:opaquestring", true, Range::InvalidRange(), Range(0, 12)},
      {"host.com/path/file", false, Range(0, 8), Range::InvalidRange()},
      {"This is plain text", true, Range::InvalidRange(),
       Range::InvalidRange()},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.input);

    SetAndEmphasizeText(test_case.input, false);
    EXPECT_EQ(test_case.expected_base_text_emphasized,
              omnibox_view()->base_text_emphasis());
    EXPECT_EQ(test_case.expected_emphasis_range,
              omnibox_view()->emphasis_range());
    EXPECT_FALSE(omnibox_view()->scheme_range().IsValid());

    if (test_case.expected_scheme_range.IsValid()) {
      SetAndEmphasizeText(test_case.input, true);
      EXPECT_EQ(test_case.expected_base_text_emphasized,
                omnibox_view()->base_text_emphasis());
      EXPECT_EQ(test_case.expected_emphasis_range,
                omnibox_view()->emphasis_range());
      EXPECT_EQ(test_case.expected_scheme_range,
                omnibox_view()->scheme_range());
    }
  }
}

TEST_F(OmniboxViewViewsTest, RevertOnBlur) {
  location_bar_model()->set_url(GURL("https://example.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(base::ASCIIToUTF16("https://example.com/"),
            omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Set the view text without updating the model's user text. This usually
  // occurs when the omnibox unapplies Steady State Elisions to temporarily show
  // the full URL to the user.
  omnibox_view()->SetWindowTextAndCaretPos(base::ASCIIToUTF16("view text"), 0,
                                           false, false);
  EXPECT_EQ(base::ASCIIToUTF16("view text"), omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, we revert to the original text and are not in user
  // input mode.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(base::ASCIIToUTF16("https://example.com/"),
            omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  // Now set user text, which is reflected into the model as well.
  omnibox_view()->SetUserText(base::ASCIIToUTF16("user text"));
  EXPECT_EQ(base::ASCIIToUTF16("user text"), omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on blur, if the text has been edited, stay in user input mode.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(base::ASCIIToUTF16("user text"), omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsTest, RevertOnEscape) {
  location_bar_model()->set_url(GURL("https://permanent-text.com/"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  EXPECT_EQ(base::ASCIIToUTF16("https://permanent-text.com/"),
            omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  omnibox_view()->SetUserText(base::ASCIIToUTF16("user text"));
  EXPECT_EQ(base::ASCIIToUTF16("user text"), omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Expect that on Escape, the text is reverted to the permanent URL.
  ui::KeyEvent escape(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
  omnibox_textfield()->OnKeyEvent(&escape);

  EXPECT_EQ(base::ASCIIToUTF16("https://permanent-text.com/"),
            omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsTest, BackspaceExitsKeywordMode) {
  omnibox_view()->SetUserText(base::UTF8ToUTF16("user text"));
  omnibox_view()->model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);

  ASSERT_EQ(base::UTF8ToUTF16("user text"), omnibox_view()->GetText());
  ASSERT_TRUE(omnibox_view()->IsSelectAll());
  ASSERT_FALSE(omnibox_view()->model()->keyword().empty());

  // First backspace should clear the user text but not exit keyword mode.
  ui::KeyEvent backspace(ui::ET_KEY_PRESSED, ui::VKEY_BACK, 0);
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());

  // Second backspace should exit keyword mode.
  omnibox_textfield()->OnKeyEvent(&backspace);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_TRUE(omnibox_view()->model()->keyword().empty());
}

TEST_F(OmniboxViewViewsTest, BlurNeverExitsKeywordMode) {
  location_bar_model()->set_url(GURL());
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  // Enter keyword mode, but with no user text.
  omnibox_view()->model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());

  // Expect that on blur, stay in keyword mode.
  omnibox_textfield()->OnBlur();
  EXPECT_TRUE(omnibox_view()->GetText().empty());
  EXPECT_FALSE(omnibox_view()->model()->keyword().empty());
}

TEST_F(OmniboxViewViewsTest, PasteAndGoToUrlOrSearchCommand) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;
  command_updater()->UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, true);

  // Test command is disabled for an empty clipboard.
  clipboard->Clear(clipboard_buffer);
  EXPECT_FALSE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));

  // Test input that's a valid URL.
  base::string16 expected_text =
#if defined(OS_MACOSX)
      base::ASCIIToUTF16("Pa&ste and Go to https://test.com");
#else
      base::ASCIIToUTF16("Pa&ste and go to https://test.com");
#endif
  ui::ScopedClipboardWriter(clipboard_buffer)
      .WriteText(base::ASCIIToUTF16("https://test.com/"));
  base::string16 returned_text =
      omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);

  // Test input that's URL-like. (crbug.com/980002).
  expected_text =
#if defined(OS_MACOSX)
      base::ASCIIToUTF16("Pa&ste and Go to test.com");
#else
      base::ASCIIToUTF16("Pa&ste and go to test.com");
#endif
  ui::ScopedClipboardWriter(clipboard_buffer)
      .WriteText(base::ASCIIToUTF16("test.com"));
  returned_text = omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);

  // Test input that's search-like.
  expected_text =
#if defined(OS_MACOSX)
      base::WideToUTF16(
          L"Pa&ste and Search for \x201Cthis is a test sentence\x201D");
#else
      base::WideToUTF16(
          L"Pa&ste and search for \x201Cthis is a test sentence\x201D");
#endif
  ui::ScopedClipboardWriter(clipboard_buffer)
      .WriteText(base::ASCIIToUTF16("this is a test sentence"));
  returned_text = omnibox_view()->GetLabelForCommandId(IDC_PASTE_AND_GO);
  EXPECT_TRUE(omnibox_view()->IsCommandIdEnabled(IDC_PASTE_AND_GO));
  EXPECT_EQ(expected_text, returned_text);
}

// Verifies |OmniboxEditModel::State::needs_revert_and_select_all|, and verifies
// a recent regression in this logic (see https://crbug.com/923290).
TEST_F(OmniboxViewViewsTest, SelectAllOnReactivateTabAfterDeleteAll) {
  omnibox_edit_controller()->set_omnibox_view(omnibox_view());

  auto web_contents1 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Simulate a new tab with "about:blank".
  const GURL url_1("about:blank/");
  location_bar_model()->set_url(url_1);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  omnibox_view()->SaveStateToTab(web_contents1.get());

  // Simulate creating another tab at "chrome://history". The second url should
  // be longer than the first (to trigger the bug).
  auto web_contents2 =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  const GURL url_2("chrome://history/");
  EXPECT_GT(url_2.spec().size(), url_1.spec().size());
  // Notice the url is set before ResetDisplayTexts(), this matches what
  // actually happens in code.
  location_bar_model()->set_url(url_2);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  // Delete all the text.
  omnibox_view()->SetUserText(base::string16());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Switch back to the first url.
  location_bar_model()->set_url(url_1);
  omnibox_view()->SaveStateToTab(web_contents2.get());
  omnibox_view()->OnTabChanged(web_contents1.get());

  // Switch back to the second url. Even though the text was deleted earlier,
  // the previous text (|url_2|) should be restored *and* all the text selected.
  location_bar_model()->set_url(url_2);
  omnibox_view()->SaveStateToTab(web_contents1.get());
  omnibox_view()->OnTabChanged(web_contents2.get());
  EXPECT_EQ(url_2, GURL(base::UTF16ToUTF8(omnibox_view()->GetText())));
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, SelectAllDuringMouseDown) {
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::ET_MOUSE_PRESSED, {0, 0}));
  omnibox_view()->SetUserText(base::ASCIIToUTF16("abc"));
  ui::KeyEvent event_a(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
  omnibox_textfield_view()->OnKeyPressed(event_a);
  // Normally SelectAll happens after OnMouseRelease. Verifying this happens
  // during OnKeyPress when the mouse is down.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsTest, SetWindowTextAndCaretPos) {
  // googl|e.com
  omnibox_view()->SetWindowTextAndCaretPos(base::UTF8ToUTF16("google.com"), 5,
                                           false, false);
  EXPECT_EQ(base::ASCIIToUTF16("google.com"), omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{5, 5}}));
}

TEST_F(OmniboxViewViewsTest, OnInlineAutocompleteTextMaybeChanged) {
  // No selection, google.com|
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      base::UTF8ToUTF16("google.com"), 0, 10);
  EXPECT_EQ(base::ASCIIToUTF16("google.com"), omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{10, 10}}));

  // Single selection, gmai[l.com]
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      base::UTF8ToUTF16("gmail.com"), 0, 4);
  EXPECT_EQ(base::ASCIIToUTF16("gmail.com"), omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{9, 4}}));

  // Multiselection, [go]ogl[e.com]
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      base::UTF8ToUTF16("google.com"), 2, 3);
  EXPECT_EQ(base::ASCIIToUTF16("google.com"), omnibox_view()->GetText());
  EXPECT_EQ(omnibox_view()->GetRenderText()->GetAllSelections(),
            (std::vector<Range>{{10, 5}, {0, 2}}));
}

class OmniboxViewViewsClipboardTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<ui::TextEditCommand> {
 public:
  void SetUp() override {
    OmniboxViewViewsTest::SetUp();

    location_bar_model()->set_url(GURL("https://test.com/"));
    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
  }
};

TEST_P(OmniboxViewViewsClipboardTest, ClipboardCopyOrCutURL) {
  omnibox_view()->SelectAll(false);
  ASSERT_TRUE(omnibox_view()->IsSelectAll());

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;

  clipboard->Clear(clipboard_buffer);
  ui::TextEditCommand clipboard_command = GetParam();
  textfield_test_api()->ExecuteTextEditCommand(clipboard_command);

  base::string16 expected_text;
  if (clipboard_command == ui::TextEditCommand::COPY)
    expected_text = base::ASCIIToUTF16("https://test.com/");
  EXPECT_EQ(expected_text, omnibox_view()->GetText());

  // Make sure the plain text format is available, but the HTML one isn't.
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::GetPlainTextType(), clipboard_buffer));
  EXPECT_FALSE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::GetHtmlType(), clipboard_buffer));

  // Windows clipboard only supports text URLs.
  // Mac clipboard not reporting URL format available for some reason.
  // crbug.com/751031
#if defined(OS_LINUX)
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::GetUrlType(), clipboard_buffer));
#endif

  std::string read_from_clipboard;
  clipboard->ReadAsciiText(clipboard_buffer, &read_from_clipboard);
  EXPECT_EQ("https://test.com/", read_from_clipboard);
}

TEST_P(OmniboxViewViewsClipboardTest, ClipboardCopyOrCutUserText) {
  omnibox_view()->SetUserText(base::ASCIIToUTF16("user text"));
  omnibox_view()->SelectAll(false);
  ASSERT_TRUE(omnibox_view()->IsSelectAll());

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::ClipboardBuffer clipboard_buffer = ui::ClipboardBuffer::kCopyPaste;

  clipboard->Clear(clipboard_buffer);
  ui::TextEditCommand clipboard_command = GetParam();
  textfield_test_api()->ExecuteTextEditCommand(clipboard_command);

  if (clipboard_command == ui::TextEditCommand::CUT)
    EXPECT_EQ(base::string16(), omnibox_view()->GetText());

  // Make sure HTML format isn't written. See
  // BookmarkNodeData::WriteToClipboard() for details.
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::GetPlainTextType(), clipboard_buffer));
  EXPECT_FALSE(clipboard->IsFormatAvailable(
      ui::ClipboardFormatType::GetHtmlType(), clipboard_buffer));

  std::string read_from_clipboard;
  clipboard->ReadAsciiText(clipboard_buffer, &read_from_clipboard);
  EXPECT_EQ("user text", read_from_clipboard);
}

INSTANTIATE_TEST_SUITE_P(OmniboxViewViewsClipboardTest,
                         OmniboxViewViewsClipboardTest,
                         ::testing::Values(ui::TextEditCommand::COPY,
                                           ui::TextEditCommand::CUT));

class OmniboxViewViewsSteadyStateElisionsTest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsSteadyStateElisionsTest()
      : OmniboxViewViewsTest(
            {
                omnibox::kHideSteadyStateUrlScheme,
                omnibox::kHideSteadyStateUrlTrivialSubdomains,
            },
            {}) {}

 protected:
  const int kCharacterWidth = 10;
  const GURL kFullUrl = GURL("https://www.example.com/");

  void SetUp() override {
    OmniboxViewViewsTest::SetUp();

    // Advance 5 seconds from epoch so the time is not considered null.
    clock_.Advance(base::TimeDelta::FromSeconds(5));
    ui::SetEventTickClockForTesting(&clock_);

    location_bar_model()->set_url(kFullUrl);
    location_bar_model()->set_url_for_display(
        base::ASCIIToUTF16("example.com"));

    gfx::test::RenderTextTestApi render_text_test_api(
        omnibox_view()->GetRenderText());
    render_text_test_api.SetGlyphWidth(kCharacterWidth);

    omnibox_view()->model()->ResetDisplayTexts();
    omnibox_view()->RevertAll();
  }

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    OmniboxViewViewsTest::TearDown();
  }

  void BlurOmnibox() {
    ASSERT_TRUE(omnibox_view()->HasFocus());
    omnibox_view()->GetFocusManager()->ClearFocus();
    ASSERT_FALSE(omnibox_view()->HasFocus());
  }

  void ExpectFullUrlDisplayed() {
    EXPECT_EQ(base::UTF8ToUTF16(kFullUrl.spec()), omnibox_view()->GetText());
    EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());
  }

  bool IsElidedUrlDisplayed() {
    return omnibox_view()->GetText() == base::ASCIIToUTF16("example.com") &&
           !omnibox_view()->model()->user_input_in_progress();
  }

  // Gets a point at |x_offset| from the beginning of the RenderText.
  gfx::Point GetPointInTextAtXOffset(int x_offset) {
    gfx::Rect bounds = omnibox_view()->GetRenderText()->display_rect();
    return gfx::Point(bounds.x() + x_offset, bounds.y() + bounds.height() / 2);
  }

  // Sends a mouse down and mouse up event at |x_offset| pixels from the
  // beginning of the RenderText.
  void SendMouseClick(int x_offset) {
    gfx::Point point = GetPointInTextAtXOffset(x_offset);
    SendMouseClickAtPoint(point, 1);
  }

  // Sends a mouse down and mouse up event at a point
  // beginning of the RenderText.
  void SendMouseClickAtPoint(gfx::Point point,
                             int click_count,
                             int event_flags = ui::EF_LEFT_MOUSE_BUTTON) {
    auto mouse_pressed =
        CreateMouseEvent(ui::ET_MOUSE_PRESSED, point, event_flags);
    mouse_pressed.SetClickCount(click_count);
    omnibox_textfield()->OnMousePressed(mouse_pressed);
    auto mouse_released =
        CreateMouseEvent(ui::ET_MOUSE_RELEASED, point, event_flags);
    mouse_released.SetClickCount(click_count);
    omnibox_textfield()->OnMouseReleased(mouse_released);
  }

  // Used to access members that are marked private in views::TextField.
  base::SimpleTestTickClock* clock() { return &clock_; }

 private:
  base::SimpleTestTickClock clock_;
};

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UrlStartsInElidedState) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideOnArrowKey) {
  SendMouseClick(0);

  // Right key should unelide and move the cursor to the end.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, 0));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(23U, start);
  EXPECT_EQ(23U, end);

  // Blur to restore the elided URL, then click on the Omnibox again to refocus.
  BlurOmnibox();
  SendMouseClick(0);

  // Left key should unelide and move the cursor to the beginning of the elided
  // part.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, 0));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(12U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideOnHomeKey) {
  SendMouseClick(0);

  // Home key should unelide and move the cursor to the beginning of the full
  // unelided URL.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_HOME, 0));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       UnelideViaEndKeyWorksWithIntranetUrls) {
  location_bar_model()->set_url(GURL("https://foobar/"));
  location_bar_model()->set_formatted_full_url(
      base::ASCIIToUTF16("https://foobar"));
  location_bar_model()->set_url_for_display(base::ASCIIToUTF16("foobar/"));

  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();

  SendMouseClick(0);

  // End key should unelide and move the cursor to the end of the full URL.
  omnibox_textfield_view()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_END, 0));

  EXPECT_EQ(base::ASCIIToUTF16("https://foobar"), omnibox_view()->GetText());
  EXPECT_FALSE(omnibox_view()->model()->user_input_in_progress());

  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(14U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, GestureTaps) {
  ui::GestureEvent tap_down(0, 0, 0, ui::EventTimeForNow(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  omnibox_textfield()->OnGestureEvent(&tap_down);

  // Select all on first tap.
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, ui::EventTimeForNow(), tap_details);
  omnibox_textfield()->OnGestureEvent(&tap);

  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Unelide on second tap (cursor placement).
  omnibox_textfield()->OnGestureEvent(&tap);
  ExpectFullUrlDisplayed();
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, FirstMouseClickFocusesOnly) {
  EXPECT_FALSE(omnibox_view()->IsSelectAll());

  SendMouseClick(0);

  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, NegligibleDragKeepsElisions) {
  gfx::Point click_point = GetPointInTextAtXOffset(2 * kCharacterWidth);
  omnibox_textfield()->OnMousePressed(
      CreateMouseEvent(ui::ET_MOUSE_PRESSED, click_point));

  // Offset the drag and release point by an insignificant 2 px.
  gfx::Point drag_point = click_point;
  drag_point.Offset(2, 0);
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::ET_MOUSE_DRAGGED, drag_point));
  omnibox_textfield()->OnMouseReleased(
      CreateMouseEvent(ui::ET_MOUSE_RELEASED, drag_point));

  // Expect that after a negligible drag and release, everything is selected.
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, CaretPlacementByMouse) {
  SendMouseClick(0);

  // Advance the clock 5 seconds so the second click is not interpreted as a
  // double click.
  clock()->Advance(base::TimeDelta::FromSeconds(5));

  // Second click should unelide only on mouse release.
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());
  omnibox_textfield()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Verify the cursor position is https://www.ex|ample.com. It should be
  // between 'x' and 'a', because the click was after the second character of
  // the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(14U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseDoubleClick) {
  SendMouseClick(4 * kCharacterWidth);

  // Second click without advancing the clock should be a double-click, which
  // should do a single word selection and unelide the text on mousedown.
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Verify that the selection is https://www.|example|.com, since the
  // double-click after the fourth character of the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseSingleThenDoubleClick) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_EQ(base::ASCIIToUTF16("example.com"), omnibox_view()->GetText());

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Advance the clock 5 seconds so the next click is not interpreted as a
  // double click.
  clock()->Advance(base::TimeDelta::FromSeconds(5));

  // Double click
  SendMouseClickAtPoint(point, 1);
  ExpectFullUrlDisplayed();
  SendMouseClickAtPoint(point, 2);
  ExpectFullUrlDisplayed();

  // Verify that the selection is https://www.|example|.com, since the
  // double-click after the fourth character of the unelided text "example.com".
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseSingleThenRightClick) {
  EXPECT_TRUE(IsElidedUrlDisplayed());
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_EQ(base::ASCIIToUTF16("example.com"), omnibox_view()->GetText());

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Advance the clock 5 seconds so the next click is not interpreted as a
  // double click.
  clock()->Advance(base::TimeDelta::FromSeconds(5));

  // Right click
  SendMouseClickAtPoint(point, 1, ui::EF_RIGHT_MOUSE_BUTTON);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  EXPECT_TRUE(omnibox_view()->HasFocus());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseTripleClick) {
  auto point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  SendMouseClickAtPoint(point, 1);
  SendMouseClickAtPoint(point, 2);
  SendMouseClickAtPoint(point, 3);

  ExpectFullUrlDisplayed();

  // Verify that the whole full URL is selected.
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(0U, start);
  EXPECT_EQ(24U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseClickDrag) {
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that during the drag, the URL is still elided.
  omnibox_textfield()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  EXPECT_TRUE(IsElidedUrlDisplayed());

  // Expect that ex|am|ple.com is the drag selected portion while dragging.
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(2U, start);
  EXPECT_EQ(4U, end);

  omnibox_textfield()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Expect that https://www.ex|am|ple.com is the selected portion after the
  // user releases the mouse.
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(14U, start);
  EXPECT_EQ(16U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       MouseClickDragToBeginningSelectingText) {
  // Backwards drag-select this portion of the elided URL: |exam|ple.com
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  omnibox_textfield()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_textfield()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Since the selection did not look like a URL, expect the following selected
  // selected portion after the user releases the mouse:
  // https://www.|exam|ple.com
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(16U, start);
  EXPECT_EQ(12U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       MouseClickDragToBeginningSelectingURL) {
  // Backwards drag-select this portion of the elided URL: |example.co|m
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(10 * kCharacterWidth)));
  omnibox_textfield()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  omnibox_textfield()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(0 * kCharacterWidth)));
  ExpectFullUrlDisplayed();

  // Since the selection does look like a URL, expect the following selected
  // selected portion after the user releases the mouse:
  // |https://www.example.co|m
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(22U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, MouseDoubleClickDrag) {
  // Expect that after a double-click after the third character of the elided
  // text, the text is unelided, and https://www.|example|.com is selected.
  SendMouseClick(4 * kCharacterWidth);
  omnibox_textfield()->OnMousePressed(CreateMouseEvent(
      ui::ET_MOUSE_PRESSED, GetPointInTextAtXOffset(4 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);

  // Expect that negligible drags are ignored immediately after unelision, as
  // the text has likely shifted, and we don't want to accidentally change the
  // selection.
  gfx::Point drag_point = GetPointInTextAtXOffset(4 * kCharacterWidth);
  drag_point.Offset(1, 1);  // Offset test point one pixel in each dimension.
  omnibox_textfield()->OnMouseDragged(
      CreateMouseEvent(ui::ET_MOUSE_DRAGGED, drag_point));
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(12U, start);
  EXPECT_EQ(19U, end);

  // Expect that dragging to the fourth character of the full URL (between the
  // the 'p' and the 's' of https), will word-select the scheme, subdomain, and
  // domain, so the new selection will be |https://www.example|.com. The
  // expected selection is backwards, since we are dragging the mouse from the
  // domain to the scheme.
  omnibox_textfield()->OnMouseDragged(CreateMouseEvent(
      ui::ET_MOUSE_DRAGGED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(19U, start);
  EXPECT_EQ(0U, end);

  // Expect the selection to stay the same after mouse-release.
  omnibox_textfield()->OnMouseReleased(CreateMouseEvent(
      ui::ET_MOUSE_RELEASED, GetPointInTextAtXOffset(2 * kCharacterWidth)));
  ExpectFullUrlDisplayed();
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(19U, start);
  EXPECT_EQ(0U, end);
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, ReelideOnBlur) {
  // Double-click should unelide the URL by making a partial selection.
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);
  ExpectFullUrlDisplayed();

  BlurOmnibox();
  EXPECT_TRUE(IsElidedUrlDisplayed());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, DontReelideOnBlurIfEdited) {
  // Double-click should unelide the URL by making a partial selection.
  SendMouseClick(4 * kCharacterWidth);
  SendMouseClick(4 * kCharacterWidth);
  ExpectFullUrlDisplayed();

  // Since the domain word is selected, pressing 'a' should replace the domain.
  ui::KeyEvent char_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0,
                          ui::DomKey::FromCharacter('a'),
                          ui::EventTimeForNow());
  omnibox_textfield()->InsertChar(char_event);
  EXPECT_EQ(base::ASCIIToUTF16("https://www.a.com/"),
            omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());

  // Now that we've edited the text, blurring should not re-elide the URL.
  BlurOmnibox();
  EXPECT_EQ(base::ASCIIToUTF16("https://www.a.com/"),
            omnibox_view()->GetText());
  EXPECT_TRUE(omnibox_view()->model()->user_input_in_progress());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest,
       DontReelideOnBlurIfWidgetDeactivated) {
  SendMouseClick(0);
  SendMouseClick(0);
  ExpectFullUrlDisplayed();

  // Create a different Widget that will take focus away from the test widget
  // containing our test Omnibox.
  std::unique_ptr<views::Widget> other_widget = CreateTestWidget();
  other_widget->Show();
  ExpectFullUrlDisplayed();

  omnibox_view()->GetWidget()->Activate();
  ExpectFullUrlDisplayed();
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, SaveSelectAllOnBlurAndRefocus) {
  SendMouseClick(0);
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());

  // Blurring and refocusing should preserve a select-all state.
  BlurOmnibox();
  omnibox_view()->RequestFocus();
  EXPECT_TRUE(omnibox_view()->HasFocus());
  EXPECT_TRUE(IsElidedUrlDisplayed());
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
}

TEST_F(OmniboxViewViewsSteadyStateElisionsTest, UnelideFromModel) {
  EXPECT_TRUE(IsElidedUrlDisplayed());

  omnibox_view()->model()->Unelide();
  EXPECT_TRUE(omnibox_view()->IsSelectAll());
  size_t start, end;
  omnibox_view()->GetSelectionBounds(&start, &end);
  EXPECT_EQ(24U, start);
  EXPECT_EQ(0U, end);
  ExpectFullUrlDisplayed();
}

// Checks that |render_text|'s current display rect and offset does not display
// |path|, and also does not display |subdomain| if
// |should_elide_to_registrable_domain| is true.
//
// |subdomain| is assumed to be a prefix of |hostname|. |subdomain| should
// include a trailing "." and |path| should include a leading "/".
void ExpectElidedToSimplifiedDomain(gfx::RenderText* render_text,
                                    const base::string16& subdomain,
                                    const base::string16& hostname,
                                    const base::string16& path,
                                    bool should_elide_to_registrable_domain) {
  gfx::Rect subdomain_rect;
  for (const auto& rect :
       render_text->GetSubstringBounds(gfx::Range(0, subdomain.size()))) {
    subdomain_rect.Union(rect);
  }
  gfx::Rect path_rect;
  for (const auto& rect : render_text->GetSubstringBounds(
           gfx::Range(hostname.size(), hostname.size() + path.size()))) {
    path_rect.Union(rect);
  }
  EXPECT_FALSE(render_text->display_rect().Contains(path_rect));
  if (should_elide_to_registrable_domain) {
    EXPECT_FALSE(render_text->display_rect().Contains(subdomain_rect));
    EXPECT_EQ(subdomain_rect.width(),
              -1 * render_text->GetUpdatedDisplayOffset().x());
  } else {
    EXPECT_TRUE(render_text->display_rect().Contains(subdomain_rect));
    EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
  }
}

// Checks that |render_text|'s current display rect and offset displays all of
// |display_url|.
void ExpectUnelidedFromSimplifiedDomain(gfx::RenderText* render_text,
                                        const base::string16& display_url) {
  gfx::Rect unelided_rect;
  for (const auto& rect :
       render_text->GetSubstringBounds(gfx::Range(0, display_url.size()))) {
    unelided_rect.Union(rect);
  }
  EXPECT_TRUE(render_text->display_rect().Contains(unelided_rect));
  EXPECT_EQ(0, render_text->GetUpdatedDisplayOffset().x());
}

// Returns true if |render_text|'s current display rect and offset display at
// least part of |path_bounds|, but not the full |display_url|. This is useful
// for checking the displayed text partway through an animation.
bool IsPartlyThroughSimplifiedDomainElision(gfx::RenderText* render_text,
                                            const base::string16& display_url,
                                            const gfx::Range& path_bounds) {
  // First check if all of |display_url| is showing; if it is, we aren't partly
  // elided.
  gfx::Rect unelided_rect;
  for (const auto& rect :
       render_text->GetSubstringBounds(gfx::Range(0, display_url.size()))) {
    unelided_rect.Union(rect);
  }
  if (render_text->display_rect().Contains(unelided_rect) &&
      render_text->GetUpdatedDisplayOffset().x() == 0) {
    return false;
  }
  // Now check if at least some of |path| is visible.
  gfx::Rect path_rect;
  for (const auto& rect : render_text->GetSubstringBounds(path_bounds)) {
    path_rect.Union(rect);
  }
  return render_text->display_rect().Intersects(path_rect);
}

class OmniboxViewViewsNoSimplifiedDomainTest : public OmniboxViewViewsTest {
 public:
  OmniboxViewViewsNoSimplifiedDomainTest()
      : OmniboxViewViewsTest(
            {},
            {omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction,
             omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover}) {}

  OmniboxViewViewsNoSimplifiedDomainTest(
      const OmniboxViewViewsNoSimplifiedDomainTest&) = delete;
  OmniboxViewViewsNoSimplifiedDomainTest& operator=(
      const OmniboxViewViewsNoSimplifiedDomainTest&) = delete;
};

// Tests that when no simplified domain field trials are enabled, URL components
// are not hidden. Regression test for https://crbug.com/1093748.
TEST_F(OmniboxViewViewsNoSimplifiedDomainTest, UrlNotSimplifiedByDefault) {
  SetUpSimplifiedDomainTest();
  omnibox_view()->EmphasizeURLComponents();
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kSimplifiedDomainDisplayUrl));
}

class OmniboxViewViewsRevealOnHoverTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OmniboxViewViewsRevealOnHoverTest()
      : OmniboxViewViewsTest(
            GetParam()
                ? std::vector<base::Feature>(
                      {omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover,
                       omnibox::kElideToRegistrableDomain})
                : std::vector<base::Feature>(
                      {omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover}),
            {}) {}

  OmniboxViewViewsRevealOnHoverTest(const OmniboxViewViewsRevealOnHoverTest&) =
      delete;
  OmniboxViewViewsRevealOnHoverTest& operator=(
      const OmniboxViewViewsRevealOnHoverTest&) = delete;

 protected:
  bool ShouldElideToRegistrableDomain() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(OmniboxViewViewsRevealOnHoverTest,
                         OmniboxViewViewsRevealOnHoverTest,
                         ::testing::Values(true, false));

// Tests the field trial variation that shows a simplified domain by default and
// reveals the unsimplified URL on hover.
TEST_P(OmniboxViewViewsRevealOnHoverTest, HoverAndExit) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // As soon as the mouse hovers over the omnibox, the unelide animation should
  // start running.
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* hover_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(hover_animation);
  EXPECT_TRUE(hover_animation->IsAnimating());

  // Advance the clock through the animation.
  gfx::AnimationContainerElement* hover_animation_as_element =
      static_cast<gfx::AnimationContainerElement*>(
          hover_animation->GetAnimationForTesting());
  hover_animation_as_element->SetStartTime(base::TimeTicks());
  hover_animation_as_element->Step(
      base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(
          OmniboxFieldTrial::UnelideURLOnHoverThresholdMs()));
  // After the extended hover threshold has elapsed, the display text shouldn't
  // have changed yet.
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // Now advance through the unelision and check the display text. We assume
  // that the animation takes less than 1 second.
  hover_animation_as_element->Step(base::TimeTicks() +
                                   base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      render_text, kSimplifiedDomainDisplayUrl));
  EXPECT_FALSE(hover_animation->IsAnimating());

  // Now exit the mouse. At this point the elision animation should run.
  omnibox_view()->OnMouseExited(CreateMouseEvent(ui::ET_MOUSE_EXITED, {0, 0}));
  EXPECT_TRUE(hover_animation->IsAnimating());

  hover_animation_as_element = static_cast<gfx::AnimationContainerElement*>(
      hover_animation->GetAnimationForTesting());
  hover_animation_as_element->SetStartTime(base::TimeTicks());
  // We assume that the animation takes less than 1 second.
  hover_animation_as_element->Step(base::TimeTicks() +
                                   base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));
}

class OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest()
      : OmniboxViewViewsTest(
            GetParam()
                ? std::vector<base::Feature>(
                      {omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover,
                       omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction,
                       omnibox::kElideToRegistrableDomain})
                : std::vector<base::Feature>(
                      {omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover,
                       omnibox::
                           kHideSteadyStateUrlPathQueryAndRefOnInteraction}),
            {}) {}

  OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest(
      const OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest&) = delete;
  OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest& operator=(
      const OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest&) = delete;

 protected:
  bool ShouldElideToRegistrableDomain() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
                         OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
                         ::testing::Values(true, false));

// Tests the field trial variation that shows the simplified domain when the
// user interacts with the page and brings back the URL when the user hovers
// over the omnibox.
TEST_P(OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
       UserInteractionAndHover) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kSimplifiedDomainDisplayUrl));

  // Simulate a user interaction and check that the fade-out animation runs.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  EXPECT_TRUE(elide_animation->IsAnimating());

  // Advance the clock through the fade-out animation; we assume that it takes
  // less than 1s.
  gfx::AnimationContainerElement* elide_as_element =
      static_cast<gfx::AnimationContainerElement*>(
          elide_animation->GetAnimationForTesting());
  elide_as_element->SetStartTime(base::TimeTicks());
  elide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // A second user interaction should not run the animation again.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_FALSE(omnibox_view()
                   ->GetElideAfterInteractionAnimationForTesting()
                   ->IsAnimating());

  // The URL should come back on hover.
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* unelide_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(unelide_animation);
  EXPECT_TRUE(unelide_animation->IsAnimating());
  gfx::AnimationContainerElement* unelide_as_element =
      static_cast<gfx::AnimationContainerElement*>(
          unelide_animation->GetAnimationForTesting());
  unelide_as_element->SetStartTime(base::TimeTicks());
  // Assume that the extended hover time + fade-in time is less than 2 seconds.
  unelide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(2));
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kSimplifiedDomainDisplayUrl));
}

class OmniboxViewViewsHideOnInteractionTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OmniboxViewViewsHideOnInteractionTest()
      : OmniboxViewViewsTest(
            GetParam()
                ? std::vector<base::Feature>(
                      {omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction,
                       omnibox::kElideToRegistrableDomain})
                : std::vector<base::Feature>(
                      {omnibox::
                           kHideSteadyStateUrlPathQueryAndRefOnInteraction}),
            {}) {}
  OmniboxViewViewsHideOnInteractionTest(
      const OmniboxViewViewsHideOnInteractionTest&) = delete;
  OmniboxViewViewsHideOnInteractionTest& operator=(
      const OmniboxViewViewsHideOnInteractionTest&) = delete;

 protected:
  bool ShouldElideToRegistrableDomain() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(OmniboxViewViewsHideOnInteractionTest,
                         OmniboxViewViewsHideOnInteractionTest,
                         ::testing::Values(true, false));

// Tests the the "Always Show Full URLs" option works with the field trial
// variation that shows a simplified domain when the user interacts with the
// page.
TEST_P(OmniboxViewViewsHideOnInteractionTest, AlwaysShowFullURLs) {
  // This test does setup itself and doesn't call SetUpSimplifiedDomainTest()
  // because SetUpSimplifiedDomainTest() uses a URL with a foo.example.test
  // hostname, and in this test we want to use a "www." subdomain to test that
  // the URL is displayed properly when trivial subdomain elision is disabled.
  const base::string16 kFullUrl =
      base::ASCIIToUTF16("https://www.example.test/foo");
  location_bar_model()->set_url(GURL("https://example.test/foo"));
  location_bar_model()->set_url_for_display(kFullUrl);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  omnibox_view()->OnThemeChanged();

  // Enable the "Always show full URLs" setting.
  location_bar_model()->set_should_prevent_elision(true);
  omnibox_view()->OnShouldPreventElisionChanged();
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  omnibox_view()->OnTabChanged(web_contents.get());
  EXPECT_EQ(kFullUrl, omnibox_view()->GetText());
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kFullUrl));

  // When the Always Show Full URLs pref is enabled, the omnibox view won't
  // observe user interactions and elide the URL.
  EXPECT_FALSE(omnibox_view()->web_contents());
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  EXPECT_FALSE(elide_animation);
}

// Tests the the "Always Show Full URLs" option works with the field trial
// variation that shows a simplified domain until the user hovers over the
// omnibox.
TEST_P(OmniboxViewViewsRevealOnHoverTest, AlwaysShowFullURLs) {
  SetUpSimplifiedDomainTest();

  // Enable the "Always show full URLs" setting.
  location_bar_model()->set_should_prevent_elision(true);
  omnibox_view()->OnShouldPreventElisionChanged();

  // After a hover, there should be no animations running.
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  EXPECT_FALSE(elide_animation);
  omnibox_view()->OnMouseExited(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  elide_animation = omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  EXPECT_FALSE(elide_animation);
}

// This test fixture enables the reveal-on-hover simplified domain field trial,
// and the hide-on-interaction variation when the parameter is true.
class OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest
    : public OmniboxViewViewsTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest()
      : OmniboxViewViewsTest(
            GetParam()
                ? std::vector<base::Feature>(
                      {omnibox::kOmniboxContextMenuShowFullUrls,
                       omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover,
                       omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction,
                       omnibox::kElideToRegistrableDomain})
                : std::vector<base::Feature>(
                      {omnibox::kOmniboxContextMenuShowFullUrls,
                       omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover,
                       omnibox::kElideToRegistrableDomain}),
            {}) {}

  OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest(
      const OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest&) =
      delete;
  OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest& operator=(
      const OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest&) =
      delete;

 protected:
  bool IsHideOnInteractionEnabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest,
    OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest,
    ::testing::Values(true, false));

// Tests that unsetting the "Always show full URLs" option begins showing/hiding
// the full URL appropriately when simplified domain field trials are enabled.
// For simplicity, this test always enables kElideToRegistrableDomain; the logic
// being test isn't really affected by whether or not subdomains are hidden.
TEST_P(OmniboxViewViewsRevealOnHoverAndMaybeHideOnInteractionTest,
       UnsetAlwaysShowFullURLs) {
  // This test does setup itself and doesn't call SetUpSimplifiedDomainTest()
  // because SetUpSimplifiedDomainTest() uses a URL with a foo.example.test
  // hostname, and in this test we want to use a "www." subdomain to test that
  // the URL is displayed properly when trivial subdomain elision is disabled.
  const base::string16 kFullUrl =
      base::ASCIIToUTF16("https://www.example.test/foo");
  location_bar_model()->set_url(GURL("https://example.test/foo"));
  location_bar_model()->set_url_for_display(kFullUrl);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  omnibox_view()->OnThemeChanged();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  // Enable the "Always show full URLs" setting.
  location_bar_model()->set_should_prevent_elision(true);
  omnibox_view()->OnShouldPreventElisionChanged();
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  omnibox_view()->OnTabChanged(web_contents.get());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.test/foo"),
            omnibox_view()->GetText());

  // Now toggle the preference and check that the animations run as expected.
  location_bar_model()->set_should_prevent_elision(false);
  location_bar_model()->set_url_for_display(
      base::ASCIIToUTF16("example.test/foo"));
  omnibox_view()->OnShouldPreventElisionChanged();
  // Note: this doesn't test that LocationBarModel correctly applies the
  // preference; it simply tests that
  // OmniboxViewViews::OnShouldPreventElisionChanged retrieves an updated
  // display URL from LocationBarModel.
  EXPECT_EQ(base::ASCIIToUTF16("example.test/foo"), omnibox_view()->GetText());
  if (IsHideOnInteractionEnabled()) {
    ExpectUnelidedFromSimplifiedDomain(render_text, omnibox_view()->GetText());
    // Simulate a user interaction and check the fade-out animation.
    omnibox_view()->DidGetUserInteraction(
        blink::WebInputEvent::Type::kGestureScrollBegin);
    OmniboxViewViews::ElideAnimation* elide_animation =
        omnibox_view()->GetElideAfterInteractionAnimationForTesting();
    ASSERT_TRUE(elide_animation);
    EXPECT_TRUE(elide_animation->IsAnimating());
  } else {
    ExpectElidedToSimplifiedDomain(
        render_text, base::ASCIIToUTF16(""), base::ASCIIToUTF16("example.test"),
        base::ASCIIToUTF16("/foo"),
        true /* should elide to registrable domain */);
  }
  // Simulate a hover event and check the elide/unelide animations. This
  // should happen the same regardless of whether hide-on-interaction is
  // enabled.
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
  omnibox_view()->OnMouseExited(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  elide_animation = omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
}

// Tests that in the hide-on-interaction field trial, when the path changes
// while being elided, the animation is stopped.
TEST_P(OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
       PathChangeDuringAnimation) {
  SetUpSimplifiedDomainTest();

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kSimplifiedDomainDisplayUrl));

  // Simulate a user interaction and check that the fade-out animation runs.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  EXPECT_TRUE(elide_animation->IsAnimating());

  // Change the path and check that the animation is cancelled.
  location_bar_model()->set_url(GURL("https://foo.example.test/bar#bar"));
  location_bar_model()->set_url_for_display(
      base::ASCIIToUTF16("foo.example.test/bar#bar"));
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  EXPECT_FALSE(elide_animation->IsAnimating());
}

// Tests that vertical and horizontal positioning doesn't change when eliding
// to/from simplified domain. Regression test for https://crbug.com/1101674.
TEST_P(OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
       VerticalAndHorizontalPosition) {
  const base::string16 kDisplayUrl = base::ASCIIToUTF16("foo.example.test/bar");
  location_bar_model()->set_url(GURL("https://foo.example.test/bar"));
  location_bar_model()->set_url_for_display(kDisplayUrl);
  omnibox_view()->model()->ResetDisplayTexts();
  omnibox_view()->RevertAll();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  // Call OnThemeChanged() to create the animations.
  omnibox_view()->OnThemeChanged();

  const gfx::Rect& original_display_rect = render_text->display_rect();

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kDisplayUrl));

  // After a navigation, the URL should not be elided to the simplified domain,
  // and the display rect (including vertical and horizontal position) should be
  // unchanged.
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      omnibox_view()->GetRenderText(), kDisplayUrl));
  EXPECT_EQ(original_display_rect, render_text->display_rect());

  // Simulate a user interaction to elide to simplified domain and advance
  // through the animation; the vertical position should still be unchanged, and
  // the text should still start at the some position (the same x value).
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  elide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  const gfx::Rect& elided_display_rect = render_text->display_rect();
  EXPECT_EQ(original_display_rect.y(), elided_display_rect.y());
  EXPECT_EQ(original_display_rect.height(), elided_display_rect.height());
  EXPECT_EQ(original_display_rect.x(), elided_display_rect.x());

  // Now hover over the omnibox to trigger an unelide and check that the display
  // rect (including vertical and horizontal position) is back to what it was
  // originally.
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* unelide_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(unelide_animation);
  gfx::AnimationContainerElement* unelide_as_element =
      static_cast<gfx::AnimationContainerElement*>(
          unelide_animation->GetAnimationForTesting());
  unelide_as_element->SetStartTime(base::TimeTicks());
  unelide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  const gfx::Rect& unelided_display_rect = render_text->display_rect();
  EXPECT_EQ(original_display_rect, unelided_display_rect);
}

// Tests that in the hide-on-interaction field trial, the URL is simplified on
// cross-document main-frame navigations, but not on same-document navigations.
TEST_P(OmniboxViewViewsHideOnInteractionTest, SameDocNavigations) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(false);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
        render_text, kSimplifiedDomainDisplayUrl));
  }

  // On a same-document navigation before the URL has been simplified, the URL
  // should remain unsimplified.
  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(true);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
        render_text, kSimplifiedDomainDisplayUrl));
    OmniboxViewViews::ElideAnimation* elide_animation =
        omnibox_view()->GetElideAfterInteractionAnimationForTesting();
    EXPECT_FALSE(elide_animation);
  }

  // Simulate a user interaction to elide to the simplified domain.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());

  // On a cross-document main-frame navigation, the unsimplified URL should
  // remain visible.
  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(false);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
        render_text, kSimplifiedDomainDisplayUrl));
    OmniboxViewViews::ElideAnimation* elide_animation =
        omnibox_view()->GetElideAfterInteractionAnimationForTesting();
    EXPECT_FALSE(elide_animation);
  }

  // Simulate another user interaction to elide to the simplified domain, and
  // advance the clock all the way through the animation.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  elide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // On a subsequent same-document main-frame navigation, the URL should remain
  // elided to the simplified domain.
  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(true);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
        render_text, base::ASCIIToUTF16("foo."),
        base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
        ShouldElideToRegistrableDomain()));
    OmniboxViewViews::ElideAnimation* elide_animation =
        omnibox_view()->GetElideAfterInteractionAnimationForTesting();
    ASSERT_TRUE(elide_animation);
    EXPECT_FALSE(elide_animation->IsAnimating());
  }
}

// Tests that in the hide-on-interaction field trial, a same-document navigation
// does not interfere with an animation that is currently running.
TEST_P(OmniboxViewViewsHideOnInteractionTest,
       SameDocNavigationDuringAnimation) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  gfx::Range path_bounds(
      kSimplifiedDomainDisplayUrl.find(base::ASCIIToUTF16("/bar")),
      kSimplifiedDomainDisplayUrl.size());

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      render_text, kSimplifiedDomainDisplayUrl));

  // Simulate a user interaction to begin animating to the simplified domain.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());

  // Advance the clock by 1ms until the full URL is no longer showing, but we
  // haven't finished eliding to the simplified domain yet. After a
  // same-document navigation, we check that the URL is still in the same state
  // (midway through elision) and that the animation is still running
  // undisturbed. In other words, a same-document navigation shouldn't change
  // anything when an animation is in progress.
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  bool is_midway_through_elision = false;
  uint32_t step = 1;
  while (!is_midway_through_elision) {
    elide_as_element->Step(base::TimeTicks() +
                           base::TimeDelta::FromMilliseconds(++step));
    is_midway_through_elision = IsPartlyThroughSimplifiedDomainElision(
        render_text, kSimplifiedDomainDisplayUrl, path_bounds);
  }
  double animation_value =
      elide_animation->GetAnimationForTesting()->GetCurrentValue();

  content::MockNavigationHandle same_doc_navigation;
  same_doc_navigation.set_is_same_document(true);
  omnibox_view()->DidFinishNavigation(&same_doc_navigation);
  elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
  EXPECT_EQ(animation_value,
            elide_animation->GetAnimationForTesting()->GetCurrentValue());
  // The current display text should reflect that the animation in progress: the
  // full display URL shouldn't be still visible, but we haven't necessarily
  // reached the end state (just the simplified domain visible) yet.
  EXPECT_TRUE(IsPartlyThroughSimplifiedDomainElision(
      render_text, kSimplifiedDomainDisplayUrl, path_bounds));
}

// Tests that in the hide-on-interaction field trial, a second user interaction
// does not interfere with an animation that is currently running. This is
// similar to SameDocNavigationDuringAnimation except that this test checks that
// a second user interaction (rather than a same-doc navigation) lets the
// animation proceed undisturbed.
TEST_P(OmniboxViewViewsHideOnInteractionTest, UserInteractionDuringAnimation) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  gfx::Range path_bounds(
      kSimplifiedDomainDisplayUrl.find(base::ASCIIToUTF16("/bar")),
      kSimplifiedDomainDisplayUrl.size());

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);
  ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
      render_text, kSimplifiedDomainDisplayUrl));

  // Simulate a user interaction to begin animating to the simplified domain.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());

  // Advance the clock by 1ms until the full URL is no longer showing, but we
  // haven't finished eliding to the simplified domain yet. After a subsequent
  // user interaction, we check that the URL is still in the same state (midway
  // through elision) and that the animation is still running undisturbed. In
  // other words, a second user interaction shouldn't change anything when an
  // animation is in progress.
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  bool is_midway_through_elision = false;
  uint32_t step = 1;
  while (!is_midway_through_elision) {
    elide_as_element->Step(base::TimeTicks() +
                           base::TimeDelta::FromMilliseconds(++step));
    is_midway_through_elision = IsPartlyThroughSimplifiedDomainElision(
        render_text, kSimplifiedDomainDisplayUrl, path_bounds);
  }
  double animation_value =
      elide_animation->GetAnimationForTesting()->GetCurrentValue();

  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
  EXPECT_EQ(animation_value,
            elide_animation->GetAnimationForTesting()->GetCurrentValue());
  // The current display text should reflect that the animation in progress: the
  // full display URL shouldn't be still visible, but we haven't necessarily
  // reached the end state (just the simplified domain visible) yet.
  EXPECT_TRUE(IsPartlyThroughSimplifiedDomainElision(
      render_text, kSimplifiedDomainDisplayUrl, path_bounds));
}

// Tests that in the hide-on-interaction field trial, the path is not re-shown
// on subframe navigations.
TEST_P(OmniboxViewViewsHideOnInteractionTest, SubframeNavigations) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(false);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectUnelidedFromSimplifiedDomain(
        render_text, kSimplifiedDomainDisplayUrl));
  }

  // Simulate a user interaction to elide to the simplified domain, and advance
  // the clock all the way through the animation.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  elide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // On a subframe navigation, the URL should remain elided to a simplified
  // domain.
  {
    content::MockNavigationHandle navigation;
    navigation.set_is_same_document(false);
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::RenderFrameHostTester::For(web_contents->GetMainFrame())
        ->InitializeRenderFrameIfNeeded();
    content::RenderFrameHost* subframe =
        content::RenderFrameHostTester::For(web_contents->GetMainFrame())
            ->AppendChild("subframe");
    navigation.set_render_frame_host(subframe);
    omnibox_view()->DidFinishNavigation(&navigation);
    ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
        render_text, base::ASCIIToUTF16("foo."),
        base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
        ShouldElideToRegistrableDomain()));
  }
}

// Tests that in the hide-on-interaction field trial variation, the path is
// faded out after omnibox focus and blur.
TEST_P(OmniboxViewViewsHideOnInteractionAndRevealOnHoverTest,
       HideOnInteractionAfterFocusAndBlur) {
  SetUpSimplifiedDomainTest();
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();

  content::MockNavigationHandle navigation;
  navigation.set_is_same_document(false);
  omnibox_view()->DidFinishNavigation(&navigation);

  // Simulate a user interaction to fade out the path.
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  gfx::AnimationContainerElement* elide_as_element =
      elide_animation->GetAnimationForTesting();
  elide_as_element->SetStartTime(base::TimeTicks());
  elide_as_element->Step(base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      render_text, base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));

  // Now focus and blur the omnibox. After blur, the path should fade out again
  // after another user interaction.
  omnibox_view()->OnFocus();
  omnibox_view()->OnBlur();
  omnibox_view()->DidGetUserInteraction(
      blink::WebInputEvent::Type::kGestureScrollBegin);
  elide_animation =
      omnibox_view()->GetElideAfterInteractionAnimationForTesting();
  EXPECT_TRUE(elide_animation->IsAnimating());
}

// Tests that in the reveal-on-hover field trial variation (without
// hide-on-interaction), the path is faded back in after focus, then blur, then
// hover.
TEST_P(OmniboxViewViewsRevealOnHoverTest, AfterBlur) {
  SetUpSimplifiedDomainTest();

  // Focus and blur the omnibox, then hover over it. The URL should unelide.
  omnibox_view()->OnFocus();
  omnibox_view()->OnBlur();
  ASSERT_NO_FATAL_FAILURE(ExpectElidedToSimplifiedDomain(
      omnibox_view()->GetRenderText(), base::ASCIIToUTF16("foo."),
      base::ASCIIToUTF16("foo.example.test"), base::ASCIIToUTF16("/bar"),
      ShouldElideToRegistrableDomain()));
  omnibox_view()->OnMouseMoved(CreateMouseEvent(ui::ET_MOUSE_MOVED, {0, 0}));
  OmniboxViewViews::ElideAnimation* elide_animation =
      omnibox_view()->GetHoverElideOrUnelideAnimationForTesting();
  ASSERT_TRUE(elide_animation);
  EXPECT_TRUE(elide_animation->IsAnimating());
}

// TODO (manukh) move up to where the other OmniboxViewViewsTest tests are.
TEST_F(OmniboxViewViewsTest, OverflowingAutocompleteText) {
  // Make the Omnibox narrow so it can't fit the entire string (~650px), but
  // wide enough to fit the user text (~65px).
  int kOmniboxWidth = 100;
  gfx::RenderText* render_text = omnibox_view()->GetRenderText();
  render_text->SetDisplayRect(gfx::Rect(0, 0, kOmniboxWidth, 10));

  omnibox_textfield()->OnFocus();
  omnibox_view()->OnInlineAutocompleteTextMaybeChanged(
      base::ASCIIToUTF16("user text. Followed by very long autocompleted text "
                         "that is unlikely to fit in |kOmniboxWidth|"),
      0, 10);

  // NOTE: Technically (depending on the font), this expectation could fail if
  // 'user text' doesn't fit in 100px or the entire string fits in 100px.
  EXPECT_EQ(render_text->GetUpdatedDisplayOffset().x(), 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());

  // On blur, the display should remain to the start of the text.
  omnibox_textfield()->OnBlur();
  EXPECT_EQ(render_text->GetUpdatedDisplayOffset().x(), 0);
  EXPECT_FALSE(omnibox_view()->IsSelectAll());
}
