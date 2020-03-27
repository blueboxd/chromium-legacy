// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_request_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// Identifier of pin request input views group used for focus traversal.
constexpr int kPinRequestInputGroup = 1;

constexpr int kPinRequestViewWidthDp = 340;
constexpr int kPinKeyboardHeightDp = 224;
constexpr int kPinRequestViewRoundedCornerRadiusDp = 8;
constexpr int kPinRequestViewVerticalInsetDp = 8;
// Inset for all elements except the back button.
constexpr int kPinRequestViewMainHorizontalInsetDp = 36;
// Minimum inset (= back button inset).
constexpr int kPinRequestViewHorizontalInsetDp = 8;

constexpr int kCrossSizeDp = 20;
constexpr int kBackButtonSizeDp = 36;
constexpr int kLockIconSizeDp = 24;
constexpr int kBackButtonLockIconVerticalOverlapDp = 8;
constexpr int kHeaderHeightDp =
    kBackButtonSizeDp + kLockIconSizeDp - kBackButtonLockIconVerticalOverlapDp;

constexpr int kIconToTitleDistanceDp = 24;
constexpr int kTitleToDescriptionDistanceDp = 8;
constexpr int kDescriptionToAccessCodeDistanceDp = 32;
constexpr int kAccessCodeToPinKeyboardDistanceDp = 16;
constexpr int kPinKeyboardToFooterDistanceDp = 16;
constexpr int kSubmitButtonBottomMarginDp = 28;

constexpr int kTitleFontSizeDeltaDp = 4;
constexpr int kTitleLineWidthDp = 268;
constexpr int kTitleLineHeightDp = 24;
constexpr int kTitleMaxLines = 4;
constexpr int kDescriptionFontSizeDeltaDp = 0;
constexpr int kDescriptionLineWidthDp = 268;
constexpr int kDescriptionTextLineHeightDp = 18;
constexpr int kDescriptionMaxLines = 4;

constexpr int kAccessCodeFlexLengthWidthDp = 192;
constexpr int kAccessCodeFlexUnderlineThicknessDp = 1;
constexpr int kAccessCodeFontSizeDeltaDp = 4;
constexpr int kObscuredGlyphSpacingDp = 6;

constexpr int kAccessCodeInputFieldWidthDp = 24;
constexpr int kAccessCodeInputFieldUnderlineThicknessDp = 2;
constexpr int kAccessCodeInputFieldHeightDp =
    24 + kAccessCodeInputFieldUnderlineThicknessDp;
constexpr int kAccessCodeBetweenInputFieldsGapDp = 8;

constexpr int kArrowButtonSizeDp = 48;

constexpr int kPinRequestViewMinimumHeightDp =
    kPinRequestViewMainHorizontalInsetDp + kLockIconSizeDp +
    kIconToTitleDistanceDp + kTitleToDescriptionDistanceDp +
    kDescriptionToAccessCodeDistanceDp + kAccessCodeInputFieldHeightDp +
    kAccessCodeToPinKeyboardDistanceDp + kPinKeyboardToFooterDistanceDp +
    kArrowButtonSizeDp + kPinRequestViewMainHorizontalInsetDp;  // = 266

constexpr int kAlpha70Percent = 178;
constexpr int kAlpha74Percent = 189;

constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr SkColor kErrorColor = gfx::kGoogleRed300;
constexpr SkColor kArrowButtonColor = SkColorSetARGB(0x2B, 0xFF, 0xFF, 0xFF);

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

PinRequest::PinRequest() = default;
PinRequest::PinRequest(PinRequest&&) = default;
PinRequest& PinRequest::operator=(PinRequest&&) = default;
PinRequest::~PinRequest() = default;

// Label button that displays focus ring.
class PinRequestView::FocusableLabelButton : public views::LabelButton {
 public:
  FocusableLabelButton(views::ButtonListener* listener,
                       const base::string16& text)
      : views::LabelButton(listener, text) {
    SetInstallFocusRingOnFocus(true);
    focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  }

  FocusableLabelButton(const FocusableLabelButton&) = delete;
  FocusableLabelButton& operator=(const FocusableLabelButton&) = delete;
  ~FocusableLabelButton() override = default;
};

class PinRequestView::AccessCodeInput : public views::View,
                                        public views::TextfieldController {
 public:
  AccessCodeInput() = default;

  ~AccessCodeInput() override = default;

  // Deletes the last character.
  virtual void Backspace() = 0;

  // Appends a digit to the code.
  virtual void InsertDigit(int value) = 0;

  // Returns access code as string.
  virtual base::Optional<std::string> GetCode() const = 0;

  // Sets the color of the input text.
  virtual void SetInputColor(SkColor color) = 0;

  virtual void SetInputEnabled(bool input_enabled) = 0;

  // Clears the input field(s).
  virtual void ClearInput() = 0;
};

class PinRequestView::FlexCodeInput : public AccessCodeInput {
 public:
  using OnInputChange = base::RepeatingCallback<void(bool enable_submit)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  // Builds the view for an access code that consists out of an unknown number
  // of digits. |on_input_change| will be called upon digit insertion, deletion
  // or change. |on_enter| will be called when code is complete and user presses
  // enter to submit it for validation. |on_escape| will be called when pressing
  // the escape key. |obscure_pin| determines whether the entered pin is
  // displayed as clear text or as bullet points.
  FlexCodeInput(OnInputChange on_input_change,
                OnEnter on_enter,
                OnEscape on_escape,
                bool obscure_pin)
      : on_input_change_(std::move(on_input_change)),
        on_enter_(std::move(on_enter)),
        on_escape_(std::move(on_escape)) {
    DCHECK(on_input_change_);
    DCHECK(on_enter_);
    DCHECK(on_escape_);

    SetLayoutManager(std::make_unique<views::FillLayout>());

    code_field_ = AddChildView(std::make_unique<views::Textfield>());
    code_field_->set_controller(this);
    code_field_->SetTextColor(login_constants::kAuthMethodsTextColor);
    code_field_->SetFontList(views::Textfield::GetDefaultFontList().Derive(
        kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::NORMAL));
    code_field_->SetBorder(views::CreateSolidSidedBorder(
        0, 0, kAccessCodeFlexUnderlineThicknessDp, 0, kTextColor));
    code_field_->SetBackgroundColor(SK_ColorTRANSPARENT);
    code_field_->SetFocusBehavior(FocusBehavior::ALWAYS);
    code_field_->SetPreferredSize(
        gfx::Size(kAccessCodeFlexLengthWidthDp, kAccessCodeInputFieldHeightDp));

    if (obscure_pin) {
      code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
      code_field_->SetObscuredGlyphSpacing(kObscuredGlyphSpacingDp);
    } else {
      code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
    }
  }

  FlexCodeInput(const FlexCodeInput&) = delete;
  FlexCodeInput& operator=(const FlexCodeInput&) = delete;
  ~FlexCodeInput() override = default;

  // Appends |value| to the code
  void InsertDigit(int value) override {
    DCHECK_LE(0, value);
    DCHECK_GE(9, value);
    if (code_field_->GetEnabled()) {
      code_field_->SetText(code_field_->GetText() +
                           base::NumberToString16(value));
      on_input_change_.Run(true);
    }
  }

  // Deletes the last character or the selected text.
  void Backspace() override {
    // Instead of just adjusting code_field_ text directly, fire a backspace key
    // event as this handles the various edge cases (ie, selected text).

    // views::Textfield::OnKeyPressed is private, so we call it via views::View.
    auto* view = static_cast<views::View*>(code_field_);
    view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                    ui::DomCode::BACKSPACE, ui::EF_NONE));
    view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
                                    ui::DomCode::BACKSPACE, ui::EF_NONE));
    // This triggers ContentsChanged(), which calls |on_input_change_|.
  }

  // Returns access code as string if field contains input.
  base::Optional<std::string> GetCode() const override {
    base::string16 code = code_field_->GetText();
    if (!code.length()) {
      return base::nullopt;
    }
    return base::UTF16ToUTF8(code);
  }

  // Sets the color of the input text.
  void SetInputColor(SkColor color) override {
    code_field_->SetTextColor(color);
  }

  void SetInputEnabled(bool input_enabled) override {
    code_field_->SetEnabled(input_enabled);
  }

  // Clears text in input text field.
  void ClearInput() override {
    code_field_->SetText(base::string16());
    on_input_change_.Run(false);
  }

  void RequestFocus() override { code_field_->RequestFocus(); }

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    const bool has_content = new_contents.length() > 0;
    on_input_change_.Run(has_content);
  }

  // views::TextfieldController
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    // Only handle keys.
    if (key_event.type() != ui::ET_KEY_PRESSED)
      return false;

    // Default handling for events with Alt modifier like spoken feedback.
    if (key_event.IsAltDown())
      return false;

    // FlexCodeInput class responds to a limited subset of key press events.
    // All events not handled below are sent to |code_field_|.
    const ui::KeyboardCode key_code = key_event.key_code();

    // Allow using tab for keyboard navigation.
    if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB)
      return false;

    if (key_code == ui::VKEY_RETURN) {
      if (GetCode().has_value()) {
        on_enter_.Run();
      }
      return true;
    }

    if (key_code == ui::VKEY_ESCAPE) {
      on_escape_.Run();
      return true;
    }

    // We only expect digits in the PIN, so we swallow all letters.
    if (key_code >= ui::VKEY_A && key_code <= ui::VKEY_Z)
      return true;

    return false;
  }

 private:
  views::Textfield* code_field_;

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code non-empty.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;

  // To be called when user presses escape to go back.
  OnEscape on_escape_;
};

// Accessible input field for a single digit in fixed length codes.
// Customizes field description and focus behavior.
class AccessibleInputField : public views::Textfield {
 public:
  AccessibleInputField() = default;
  ~AccessibleInputField() override = default;

  void set_accessible_description(const base::string16& description) {
    accessible_description_ = description;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::Textfield::GetAccessibleNodeData(node_data);
    // The following property setup is needed to match the custom behavior of
    // pin input. It results in the following a11y vocalizations:
    // * when input field is empty: "Next number, {current field index} of
    // {number of fields}"
    // * when input field is populated: "{value}, {current field index} of
    // {number of fields}"
    node_data->RemoveState(ax::mojom::State::kEditable);
    node_data->role = ax::mojom::Role::kListItem;
    base::string16 description = views::Textfield::GetText().empty()
                                     ? accessible_description_
                                     : GetText();
    node_data->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                  base::UTF16ToUTF8(description));
  }

  bool IsGroupFocusTraversable() const override { return false; }

  View* GetSelectedViewForGroup(int group) override {
    return parent() ? parent()->GetSelectedViewForGroup(group) : nullptr;
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      RequestFocusWithPointer(event->details().primary_pointer_type());
      return;
    }

    views::Textfield::OnGestureEvent(event);
  }

 private:
  base::string16 accessible_description_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleInputField);
};

// Digital access code input view for variable length of input codes.
// Displays a separate underscored field for every input code digit.
class PinRequestView::FixedLengthCodeInput : public AccessCodeInput {
 public:
  using OnInputChange =
      base::RepeatingCallback<void(bool last_field_active, bool complete)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  class TestApi {
   public:
    explicit TestApi(
        PinRequestView::FixedLengthCodeInput* fixed_length_code_input)
        : fixed_length_code_input_(fixed_length_code_input) {}
    ~TestApi() = default;

    views::Textfield* GetInputTextField(int index) {
      DCHECK_LT(static_cast<size_t>(index),
                fixed_length_code_input_->input_fields_.size());
      return fixed_length_code_input_->input_fields_[index];
    }

   private:
    PinRequestView::FixedLengthCodeInput* fixed_length_code_input_;
  };

  // Builds the view for an access code that consists out of |length| digits.
  // |on_input_change| will be called upon access code digit insertion, deletion
  // or change. True will be passed if the current code is complete (all digits
  // have input values) and false otherwise. |on_enter| will be called when code
  // is complete and user presses enter to submit it for validation. |on_escape|
  // will be called when pressing the escape key. |obscure_pin| determines
  // whether the entered pin is displayed as clear text or as bullet points.
  FixedLengthCodeInput(int length,
                       OnInputChange on_input_change,
                       OnEnter on_enter,
                       OnEscape on_escape,
                       bool obscure_pin)
      : on_input_change_(std::move(on_input_change)),
        on_enter_(std::move(on_enter)),
        on_escape_(std::move(on_escape)) {
    DCHECK_LT(0, length);
    DCHECK(on_input_change_);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kAccessCodeBetweenInputFieldsGapDp));
    SetGroup(kPinRequestInputGroup);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    for (int i = 0; i < length; ++i) {
      auto* field = new AccessibleInputField();
      field->set_controller(this);
      field->SetPreferredSize(gfx::Size(kAccessCodeInputFieldWidthDp,
                                        kAccessCodeInputFieldHeightDp));
      field->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
      field->SetBackgroundColor(SK_ColorTRANSPARENT);
      if (obscure_pin) {
        field->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
      } else {
        field->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
      }
      field->SetTextColor(kTextColor);
      field->SetFontList(views::Textfield::GetDefaultFontList().Derive(
          kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL));
      field->SetBorder(views::CreateSolidSidedBorder(
          0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0, kTextColor));
      field->SetGroup(kPinRequestInputGroup);
      field->set_accessible_description(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_PIN_REQUEST_NEXT_NUMBER_PROMPT));
      input_fields_.push_back(field);
      AddChildView(field);
    }
  }

  ~FixedLengthCodeInput() override = default;
  FixedLengthCodeInput(const FixedLengthCodeInput&) = delete;
  FixedLengthCodeInput& operator=(const FixedLengthCodeInput&) = delete;

  // Inserts |value| into the |active_field_| and moves focus to the next field
  // if it exists.
  void InsertDigit(int value) override {
    DCHECK_LE(0, value);
    DCHECK_GE(9, value);

    ActiveField()->SetText(base::NumberToString16(value));
    bool was_last_field = IsLastFieldActive();

    // Moving focus is delayed by using PostTask to allow for proper
    // a11y announcements. Without that some of them are skipped.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FixedLengthCodeInput::FocusNextField,
                                  weak_ptr_factory_.GetWeakPtr()));

    on_input_change_.Run(was_last_field, GetCode().has_value());
  }

  // Clears input from the |active_field_|. If |active_field| is empty moves
  // focus to the previous field (if exists) and clears input there.
  void Backspace() override {
    if (ActiveInput().empty()) {
      FocusPreviousField();
    }

    ActiveField()->SetText(base::string16());
    on_input_change_.Run(IsLastFieldActive(), false /*complete*/);
  }

  // Returns access code as string if all fields contain input.
  base::Optional<std::string> GetCode() const override {
    std::string result;
    size_t length;
    for (auto* field : input_fields_) {
      length = field->GetText().length();
      if (!length)
        return base::nullopt;

      DCHECK_EQ(1u, length);
      base::StrAppend(&result, {base::UTF16ToUTF8(field->GetText())});
    }
    return result;
  }

  // Sets the color of the input text.
  void SetInputColor(SkColor color) override {
    for (auto* field : input_fields_) {
      field->SetTextColor(color);
    }
  }

  // views::View:
  bool IsGroupFocusTraversable() const override { return false; }

  View* GetSelectedViewForGroup(int group) override { return ActiveField(); }

  void RequestFocus() override { ActiveField()->RequestFocus(); }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::View::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kGroup;
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED)
      return false;

    // Default handling for events with Alt modifier like spoken feedback.
    if (key_event.IsAltDown())
      return false;

    // FixedLengthCodeInput class responds to limited subset of key press
    // events. All key pressed events not handled below are ignored.
    const ui::KeyboardCode key_code = key_event.key_code();
    if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB) {
      // Allow using tab for keyboard navigation.
      return false;
    } else if (key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9) {
      InsertDigit(key_code - ui::VKEY_0);
    } else if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9) {
      InsertDigit(key_code - ui::VKEY_NUMPAD0);
    } else if (key_code == ui::VKEY_LEFT) {
      FocusPreviousField();
    } else if (key_code == ui::VKEY_RIGHT) {
      // Do not allow to leave empty field when moving focus with arrow key.
      if (!ActiveInput().empty())
        FocusNextField();
    } else if (key_code == ui::VKEY_BACK) {
      Backspace();
    } else if (key_code == ui::VKEY_RETURN) {
      if (GetCode().has_value())
        on_enter_.Run();
    } else if (key_code == ui::VKEY_ESCAPE) {
      on_escape_.Run();
    }

    return true;
  }

  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override {
    if (!(mouse_event.IsOnlyLeftMouseButton() ||
          mouse_event.IsOnlyRightMouseButton())) {
      return false;
    }

    // Move focus to the field that was selected with mouse input.
    for (size_t i = 0; i < input_fields_.size(); ++i) {
      if (input_fields_[i] == sender) {
        active_input_index_ = i;
        RequestFocus();
        break;
      }
    }

    return true;
  }

  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override {
    if (gesture_event.details().type() != ui::EventType::ET_GESTURE_TAP)
      return false;

    // Move focus to the field that was selected with gesture.
    for (size_t i = 0; i < input_fields_.size(); ++i) {
      if (input_fields_[i] == sender) {
        active_input_index_ = i;
        RequestFocus();
        break;
      }
    }

    return true;
  }

  // Enables/disables entering a PIN. Currently, there is no use-case the uses
  // this with fixed length PINs.
  void SetInputEnabled(bool input_enabled) override { NOTIMPLEMENTED(); }

  // Clears the PIN fields. Currently, there is no use-case the uses this with
  // fixed length PINs.
  void ClearInput() override { NOTIMPLEMENTED(); }

 private:
  // Moves focus to the previous input field if it exists.
  void FocusPreviousField() {
    if (active_input_index_ == 0)
      return;

    --active_input_index_;
    ActiveField()->RequestFocus();
  }

  // Moves focus to the next input field if it exists.
  void FocusNextField() {
    if (IsLastFieldActive())
      return;

    ++active_input_index_;
    ActiveField()->RequestFocus();
  }

  // Returns whether last input field is currently active.
  bool IsLastFieldActive() const {
    return active_input_index_ == (static_cast<int>(input_fields_.size()) - 1);
  }

  // Returns pointer to the active input field.
  AccessibleInputField* ActiveField() const {
    return input_fields_[active_input_index_];
  }

  // Returns text in the active input field.
  const base::string16& ActiveInput() const { return ActiveField()->GetText(); }

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code is complete (all digits have input value)
  // and false otherwise.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;
  // To be called when user pressed escape to close view.
  OnEscape on_escape_;

  // An active/focused input field index. Incoming digit will be inserted here.
  int active_input_index_ = 0;

  // Unowned input textfields ordered from the first to the last digit.
  std::vector<AccessibleInputField*> input_fields_;

  base::WeakPtrFactory<FixedLengthCodeInput> weak_ptr_factory_{this};
};

PinRequestView::TestApi::TestApi(PinRequestView* view) : view_(view) {
  DCHECK(view_);
}

PinRequestView::TestApi::~TestApi() = default;

LoginButton* PinRequestView::TestApi::back_button() {
  return view_->back_button_;
}

views::Label* PinRequestView::TestApi::title_label() {
  return view_->title_label_;
}

views::Label* PinRequestView::TestApi::description_label() {
  return view_->description_label_;
}

views::View* PinRequestView::TestApi::access_code_view() {
  return view_->access_code_view_;
}

views::LabelButton* PinRequestView::TestApi::help_button() {
  return view_->help_button_;
}

ArrowButtonView* PinRequestView::TestApi::submit_button() {
  return view_->submit_button_;
}

LoginPinView* PinRequestView::TestApi::pin_keyboard_view() {
  return view_->pin_keyboard_view_;
}

views::Textfield* PinRequestView::TestApi::GetInputTextField(int index) {
  return PinRequestView::FixedLengthCodeInput::TestApi(
             static_cast<PinRequestView::FixedLengthCodeInput*>(
                 view_->access_code_view_))
      .GetInputTextField(index);
}

PinRequestViewState PinRequestView::TestApi::state() const {
  return view_->state_;
}

// static
SkColor PinRequestView::GetChildUserDialogColor(bool using_blur) {
  SkColor color = AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque,
      AshColorProvider::AshColorMode::kDark);

  SkColor extracted_color =
      Shell::Get()->wallpaper_controller()->GetProminentColor(
          color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                    color_utils::SaturationRange::MUTED));

  if (extracted_color != kInvalidWallpaperColor &&
      extracted_color != SK_ColorTRANSPARENT) {
    color = color_utils::GetResultingPaintColor(
        SkColorSetA(SK_ColorBLACK, kAlpha70Percent), extracted_color);
  }

  return using_blur ? SkColorSetA(color, kAlpha74Percent) : color;
}

// TODO(crbug.com/1061008): Make dialog look good on small screens with high
// zoom factor.
PinRequestView::PinRequestView(PinRequest request, Delegate* delegate)
    : delegate_(delegate),
      on_pin_request_done_(std::move(request.on_pin_request_done)),
      pin_keyboard_always_enabled_(request.pin_keyboard_always_enabled),
      default_title_(request.title),
      default_description_(request.description),
      default_accessible_title_(request.accessible_title.empty()
                                    ? request.title
                                    : request.accessible_title) {
  // Main view contains all other views aligned vertically and centered.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kPinRequestViewVerticalInsetDp,
                  kPinRequestViewHorizontalInsetDp),
      0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kPinRequestViewRoundedCornerRadiusDp));
  layer()->SetBackgroundBlur(ShelfConfig::Get()->shelf_blur_radius());

  const int child_view_width =
      kPinRequestViewWidthDp - 2 * kPinRequestViewMainHorizontalInsetDp;

  // Header view which contains the back button that is aligned top right and
  // the lock icon which is in the bottom center.
  auto header_layout = std::make_unique<views::FillLayout>();
  auto* header = new NonAccessibleView();
  header->SetLayoutManager(std::move(header_layout));
  AddChildView(header);
  auto* header_spacer = new NonAccessibleView();
  header_spacer->SetPreferredSize(gfx::Size(0, kHeaderHeightDp));
  header->AddChildView(header_spacer);

  // Main view icon.
  auto* icon_view = new NonAccessibleView();
  icon_view->SetPreferredSize(gfx::Size(0, kHeaderHeightDp));
  auto icon_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
  icon_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  icon_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  icon_view->SetLayoutManager(std::move(icon_layout));
  header->AddChildView(icon_view);

  views::ImageView* icon = new views::ImageView();
  icon->SetPreferredSize(gfx::Size(kLockIconSizeDp, kLockIconSizeDp));
  icon->SetImage(gfx::CreateVectorIcon(kPinRequestLockIcon, SK_ColorWHITE));
  icon_view->AddChildView(icon);

  // Back button. Note that it should be the last view added to |header| in
  // order to be clickable.
  auto* back_button_view = new NonAccessibleView();
  back_button_view->SetPreferredSize(
      gfx::Size(child_view_width + 2 * (kPinRequestViewMainHorizontalInsetDp -
                                        kPinRequestViewHorizontalInsetDp),
                kHeaderHeightDp));
  auto back_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  back_button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  back_button_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  back_button_view->SetLayoutManager(std::move(back_button_layout));
  header->AddChildView(back_button_view);

  back_button_ = new LoginButton(this);
  back_button_->SetPreferredSize(
      gfx::Size(kBackButtonSizeDp, kBackButtonSizeDp));
  back_button_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  back_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCrossSizeDp, SK_ColorWHITE));
  back_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  back_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_BACK_BUTTON_ACCESSIBLE_NAME));
  back_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  back_button_view->AddChildView(back_button_);

  auto add_spacer = [&](int height) {
    auto* spacer = new NonAccessibleView();
    spacer->SetPreferredSize(gfx::Size(0, height));
    AddChildView(spacer);
  };

  add_spacer(kIconToTitleDistanceDp);

  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(kTextColor);
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  };

  // Main view title.
  title_label_ = new views::Label(default_title_, views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(kTitleMaxLines);
  title_label_->SizeToFit(kTitleLineWidthDp);
  title_label_->SetLineHeight(kTitleLineHeightDp);
  title_label_->SetFontList(gfx::FontList().Derive(
      kTitleFontSizeDeltaDp, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  decorate_label(title_label_);
  AddChildView(title_label_);

  add_spacer(kTitleToDescriptionDistanceDp);

  // Main view description.
  description_label_ =
      new views::Label(default_description_, views::style::CONTEXT_LABEL,
                       views::style::STYLE_PRIMARY);
  description_label_->SetMultiLine(true);
  description_label_->SetMaxLines(kDescriptionMaxLines);
  description_label_->SizeToFit(kDescriptionLineWidthDp);
  description_label_->SetLineHeight(kDescriptionTextLineHeightDp);
  description_label_->SetFontList(
      gfx::FontList().Derive(kDescriptionFontSizeDeltaDp, gfx::Font::NORMAL,
                             gfx::Font::Weight::NORMAL));
  decorate_label(description_label_);
  AddChildView(description_label_);

  add_spacer(kDescriptionToAccessCodeDistanceDp);

  // Access code input view.
  if (request.pin_length.has_value()) {
    CHECK_GT(request.pin_length.value(), 0);
    access_code_view_ = AddChildView(std::make_unique<FixedLengthCodeInput>(
        request.pin_length.value(),
        base::BindRepeating(&PinRequestView::OnInputChange,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::SubmitCode,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::OnBack, base::Unretained(this)),
        request.obscure_pin));
  } else {
    access_code_view_ = AddChildView(std::make_unique<FlexCodeInput>(
        base::BindRepeating(&PinRequestView::OnInputChange,
                            base::Unretained(this), false),
        base::BindRepeating(&PinRequestView::SubmitCode,
                            base::Unretained(this)),
        base::BindRepeating(&PinRequestView::OnBack, base::Unretained(this)),
        request.obscure_pin));
  }
  access_code_view_->SetFocusBehavior(FocusBehavior::ALWAYS);

  add_spacer(kAccessCodeToPinKeyboardDistanceDp);

  // Pin keyboard. Note that the keyboard's own submit button is disabled via
  // passing a null |on_submit| callback.
  pin_keyboard_view_ =
      new LoginPinView(LoginPinView::Style::kAlphanumeric,
                       base::BindRepeating(&AccessCodeInput::InsertDigit,
                                           base::Unretained(access_code_view_)),
                       base::BindRepeating(&AccessCodeInput::Backspace,
                                           base::Unretained(access_code_view_)),
                       /*on_submit=*/LoginPinView::OnPinSubmit());
  // Backspace key is always enabled and |access_code_| field handles it.
  pin_keyboard_view_->OnPasswordTextChanged(false);
  AddChildView(pin_keyboard_view_);

  add_spacer(kPinKeyboardToFooterDistanceDp);

  // Footer view contains help text button aligned to its start, submit
  // button aligned to its end and spacer view in between.
  auto* footer = new NonAccessibleView();
  footer->SetPreferredSize(gfx::Size(child_view_width, kArrowButtonSizeDp));
  auto* bottom_layout =
      footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  AddChildView(footer);

  help_button_ = new FocusableLabelButton(
      this, l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PIN_REQUEST_HELP));
  help_button_->SetPaintToLayer();
  help_button_->layer()->SetFillsBoundsOpaquely(false);
  help_button_->SetTextSubpixelRenderingEnabled(false);
  help_button_->SetEnabledTextColors(kTextColor);
  help_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  help_button_->SetVisible(request.help_button_enabled);
  footer->AddChildView(help_button_);

  auto* horizontal_spacer = new NonAccessibleView();
  footer->AddChildView(horizontal_spacer);
  bottom_layout->SetFlexForView(horizontal_spacer, 1);

  submit_button_ = new ArrowButtonView(this, kArrowButtonSizeDp);
  submit_button_->SetBackgroundColor(kArrowButtonColor);
  submit_button_->SetPreferredSize(
      gfx::Size(kArrowButtonSizeDp, kArrowButtonSizeDp));
  submit_button_->SetEnabled(false);
  submit_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  footer->AddChildView(submit_button_);
  add_spacer(kSubmitButtonBottomMarginDp);

  pin_keyboard_view_->SetVisible(PinKeyboardVisible());

  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());

  SetPreferredSize(GetPinRequestViewSize());
}

PinRequestView::~PinRequestView() = default;

void PinRequestView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(GetChildUserDialogColor(true));
  canvas->DrawRoundRect(GetContentsBounds(),
                        kPinRequestViewRoundedCornerRadiusDp, flags);
}

void PinRequestView::RequestFocus() {
  access_code_view_->RequestFocus();
}

gfx::Size PinRequestView::CalculatePreferredSize() const {
  return GetPinRequestViewSize();
}

ui::ModalType PinRequestView::GetModalType() const {
  // MODAL_TYPE_SYSTEM is used to get a semi-transparent background behind the
  // pin request view, when it is used directly on a widget. The overlay
  // consumes all the inputs from the user, so that they can only interact with
  // the pin request view while it is visible.
  return ui::MODAL_TYPE_SYSTEM;
}

views::View* PinRequestView::GetInitiallyFocusedView() {
  return access_code_view_;
}

base::string16 PinRequestView::GetAccessibleWindowTitle() const {
  return default_accessible_title_;
}

void PinRequestView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender == back_button_) {
    OnBack();
  } else if (sender == help_button_) {
    delegate_->OnHelp(GetWidget()->GetNativeWindow());
  } else if (sender == submit_button_) {
    SubmitCode();
  }
}

void PinRequestView::OnTabletModeStarted() {
  if (!pin_keyboard_always_enabled_) {
    VLOG(1) << "Showing PIN keyboard in PinRequestView";
    pin_keyboard_view_->SetVisible(true);
    // This will trigger ChildPreferredSizeChanged in parent view and Layout()
    // in view. As the result whole hierarchy will go through re-layout.
    UpdatePreferredSize();
  }
}

void PinRequestView::OnTabletModeEnded() {
  if (!pin_keyboard_always_enabled_) {
    VLOG(1) << "Hiding PIN keyboard in PinRequestView";
    DCHECK(pin_keyboard_view_);
    pin_keyboard_view_->SetVisible(false);
    // This will trigger ChildPreferredSizeChanged in parent view and Layout()
    // in view. As the result whole hierarchy will go through re-layout.
    UpdatePreferredSize();
  }
}

void PinRequestView::OnTabletControllerDestroyed() {
  tablet_mode_observer_.RemoveAll();
}

void PinRequestView::SubmitCode() {
  base::Optional<std::string> code = access_code_view_->GetCode();
  DCHECK(code.has_value());

  SubmissionResult result = delegate_->OnPinSubmitted(*code);
  switch (result) {
    case SubmissionResult::kPinAccepted: {
      std::move(on_pin_request_done_).Run(true /* success */);
      return;
    }
    case SubmissionResult::kPinError: {
      // Caller is expected to call UpdateState() to allow for customization of
      // error messages.
      return;
    }
    case SubmissionResult::kSubmitPending: {
      // Waiting on validation result - do nothing for now.
      return;
    }
  }
}

void PinRequestView::OnBack() {
  delegate_->OnBack();
  if (PinRequestWidget::Get()) {
    PinRequestWidget::Get()->Close(false /* success */);
  }
}

void PinRequestView::UpdateState(PinRequestViewState state,
                                 const base::string16& title,
                                 const base::string16& description) {
  state_ = state;
  title_label_->SetText(title);
  description_label_->SetText(description);
  UpdatePreferredSize();
  switch (state_) {
    case PinRequestViewState::kNormal: {
      access_code_view_->SetInputColor(kTextColor);
      title_label_->SetEnabledColor(kTextColor);
      return;
    }
    case PinRequestViewState::kError: {
      access_code_view_->SetInputColor(kErrorColor);
      title_label_->SetEnabledColor(kErrorColor);
      // Read out the error.
      title_label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
      return;
    }
  }
}

void PinRequestView::ClearInput() {
  access_code_view_->ClearInput();
}

void PinRequestView::SetInputEnabled(bool input_enabled) {
  access_code_view_->SetInputEnabled(input_enabled);
}

void PinRequestView::UpdatePreferredSize() {
  SetPreferredSize(CalculatePreferredSize());
  if (GetWidget())
    GetWidget()->CenterWindow(GetPreferredSize());
}

void PinRequestView::FocusSubmitButton() {
  submit_button_->RequestFocus();
}

void PinRequestView::OnInputChange(bool last_field_active, bool complete) {
  if (state_ == PinRequestViewState::kError) {
    UpdateState(PinRequestViewState::kNormal, default_title_,
                default_description_);
  }

  submit_button_->SetEnabled(complete);

  if (complete && last_field_active) {
    if (auto_submit_enabled_) {
      auto_submit_enabled_ = false;
      SubmitCode();
      return;
    }

    // Moving focus is delayed by using PostTask to allow for proper
    // a11y announcements.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PinRequestView::FocusSubmitButton,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void PinRequestView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(default_accessible_title_);
}

// If |pin_keyboard_always_enabled_| is not set, pin keyboard is only shown in
// tablet mode.
bool PinRequestView::PinKeyboardVisible() const {
  return pin_keyboard_always_enabled_ || IsTabletMode();
}

gfx::Size PinRequestView::GetPinRequestViewSize() const {
  int height = kPinRequestViewMinimumHeightDp +
               std::min(int{title_label_->GetRequiredLines()}, kTitleMaxLines) *
                   kTitleLineHeightDp +
               std::min(int{description_label_->GetRequiredLines()},
                        kDescriptionMaxLines) *
                   kDescriptionTextLineHeightDp;
  if (PinKeyboardVisible())
    height += kPinKeyboardHeightDp;
  return gfx::Size(kPinRequestViewWidthDp, height);
}

}  // namespace ash
