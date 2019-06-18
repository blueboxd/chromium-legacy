// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Identifier of parent access input views group used for focus traversal.
constexpr int kParentAccessInputGroup = 1;

// Number of digits displayed in access code input.
constexpr int kParentAccessCodePinLength = 6;

constexpr int kParentAccessViewWidthDp = 340;
constexpr int kParentAccessViewHeightDp = 340;
constexpr int kParentAccessViewTabletModeHeightDp = 580;
constexpr int kParentAccessViewRoundedCornerRadiusDp = 8;
constexpr int kParentAccessViewVerticalInsetDp = 16;
constexpr int kParentAccessViewHorizontalInsetDp = 26;

constexpr int kLockIconSizeDp = 24;

constexpr int kIconToTitleDistanceDp = 28;
constexpr int kTitleToDescriptionDistanceDp = 14;
constexpr int kDescriptionToAccessCodeDistanceDp = 28;
constexpr int kAccessCodeToPinKeyboardDistanceDp = 5;
constexpr int kPinKeyboardToFooterDistanceDp = 57;
constexpr int kPinKeyboardToFooterTabletModeDistanceDp = 17;
constexpr int kSubmitButtonBottomMarginDp = 8;

constexpr int kTitleFontSizeDeltaDp = 3;
constexpr int kDescriptionFontSizeDeltaDp = -1;
constexpr int kDescriptionTextLineHeightDp = 16;

constexpr int kAccessCodeInputFieldWidthDp = 24;
constexpr int kAccessCodeInputFieldHeightDp = 32;
constexpr int kAccessCodeInputFieldUnderlineThicknessDp = 1;
constexpr int kAccessCodeBetweenInputFieldsGapDp = 4;

constexpr int kArrowButtonSizeDp = 40;
constexpr int kArrowSizeDp = 20;

constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr SkColor kErrorColor = SkColorSetARGB(0xFF, 0xF2, 0x8B, 0x82);
constexpr SkColor kArrowButtonColor = SkColorSetARGB(0x57, 0xFF, 0xFF, 0xFF);

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

gfx::Size GetPinKeyboardToFooterSpacerSize() {
  return gfx::Size(0, IsTabletMode() ? kPinKeyboardToFooterTabletModeDistanceDp
                                     : kPinKeyboardToFooterDistanceDp);
}

gfx::Size GetParentAccessViewSize() {
  return gfx::Size(kParentAccessViewWidthDp,
                   IsTabletMode() ? kParentAccessViewTabletModeHeightDp
                                  : kParentAccessViewHeightDp);
}

base::string16 GetTitle(ParentAccessRequestReason reason) {
  int title_id;
  switch (reason) {
    case ParentAccessRequestReason::kUnlockTimeLimits:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE;
      break;
    case ParentAccessRequestReason::kChangeTime:
      title_id = IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_CHANGE_TIME;
      break;
  }
  return l10n_util::GetStringUTF16(title_id);
}

base::string16 GetDescription(ParentAccessRequestReason reason) {
  int description_id;
  switch (reason) {
    case ParentAccessRequestReason::kUnlockTimeLimits:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_DESCRIPTION;
      break;
    case ParentAccessRequestReason::kChangeTime:
      description_id = IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION;
      break;
  }
  return l10n_util::GetStringUTF16(description_id);
}

// Accessible input field. Customizes field description and focus behavior.
class AccessibleInputField : public views::Textfield {
 public:
  AccessibleInputField() = default;
  ~AccessibleInputField() override = default;

  void set_accessible_description(const base::string16& description) {
    accessible_description_ = description;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kLabelText;
    node_data->SetDescription(accessible_description_);
    node_data->SetValue(text());
  }

  bool IsGroupFocusTraversable() const override { return false; }

  View* GetSelectedViewForGroup(int group) override {
    return parent() ? parent()->GetSelectedViewForGroup(group) : nullptr;
  }

 private:
  base::string16 accessible_description_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleInputField);
};

}  // namespace

// Digital access code input view for variable length of input codes.
// Displays a separate underscored field for every input code digit.
class ParentAccessView::AccessCodeInput : public views::View,
                                          public views::TextfieldController {
 public:
  using OnInputChange = base::RepeatingCallback<void(bool complete)>;
  using OnEnter = base::RepeatingClosure;

  // Builds the view for an access code that consists out of |length| digits.
  // |on_input_change| will be called upon access code digit insertion, deletion
  // or change. True will be passed if the current code is complete (all digits
  // have input values) and false otherwise. |on_enter| will be called when code
  // is complete and user presses enter to submit it for validation.
  AccessCodeInput(int length, OnInputChange on_input_change, OnEnter on_enter)
      : on_input_change_(std::move(on_input_change)),
        on_enter_(std::move(on_enter)) {
    DCHECK_LT(0, length);
    DCHECK(on_input_change_);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kHorizontal, gfx::Insets(),
        kAccessCodeBetweenInputFieldsGapDp));
    SetGroup(kParentAccessInputGroup);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    for (int i = 0; i < length; ++i) {
      auto* field = new AccessibleInputField();
      field->set_controller(this);
      field->SetPreferredSize(gfx::Size(kAccessCodeInputFieldWidthDp,
                                        kAccessCodeInputFieldHeightDp));
      field->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
      field->SetBackgroundColor(SK_ColorTRANSPARENT);
      field->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
      field->SetTextColor(kTextColor);
      field->SetFontList(views::Textfield::GetDefaultFontList().Derive(
          kDescriptionFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL));
      field->SetBorder(views::CreateSolidSidedBorder(
          0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0, kTextColor));
      field->SetGroup(kParentAccessInputGroup);
      if (i < length - 1) {
        field->set_accessible_description(l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_PARENT_ACCESS_NEXT_NUMBER_PROMPT));
      }
      input_fields_.push_back(field);
      AddChildView(field);
    }
  }

  ~AccessCodeInput() override = default;

  // Inserts |value| into the |active_field_| and moves focus to the next field
  // if it exists.
  void InsertDigit(int value) {
    DCHECK_LE(0, value);
    DCHECK_GE(9, value);

    ActiveField()->SetText(base::NumberToString16(value));
    FocusNextField();

    on_input_change_.Run(GetCode().has_value());
  }

  // Clears input from the |active_field_|. If |active_field| is empty moves
  // focus to the previous field (if exists) and clears input there.
  void Backspace() {
    if (ActiveInput().empty()) {
      FocusPreviousField();
    }

    ActiveField()->SetText(base::string16());
    on_input_change_.Run(false);
  }

  // Returns access code as string if all fields contain input.
  base::Optional<std::string> GetCode() const {
    std::string result;
    size_t length;
    for (auto* field : input_fields_) {
      length = field->text().length();
      if (!length)
        return base::nullopt;

      DCHECK_EQ(1u, length);
      base::StrAppend(&result, {base::UTF16ToUTF8(field->text())});
    }
    return result;
  }

  // Sets the color of the input text.
  void SetInputColor(SkColor color) {
    for (auto* field : input_fields_) {
      field->SetTextColor(color);
    }
  }

  // views::View:
  bool IsGroupFocusTraversable() const override { return false; }

  View* GetSelectedViewForGroup(int group) override { return ActiveField(); }

  void RequestFocus() override { ActiveField()->RequestFocus(); }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED)
      return false;

    // AccessCodeInput class responds to limited subset of key press events.
    // All key pressed events not handled below are ignored.
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
    }

    return true;
  }

  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override {
    if (!mouse_event.IsOnlyLeftMouseButton())
      return false;

    // Move focus to the field that was selected with mouse input.
    for (size_t i = 0; i < input_fields_.size(); ++i) {
      if (input_fields_[i] == sender) {
        active_input_index_ = i;
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
        break;
      }
    }

    return true;
  }

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
    if (active_input_index_ == (static_cast<int>(input_fields_.size()) - 1))
      return;

    ++active_input_index_;
    ActiveField()->RequestFocus();
  }

  // Returns pointer to the active input field.
  views::Textfield* ActiveField() const {
    return input_fields_[active_input_index_];
  }

  // Returns text in the active input field.
  const base::string16& ActiveInput() const { return ActiveField()->text(); }

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code is complete (all digits have input value)
  // and false otherwise.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;

  // An active/focused input field index. Incoming digit will be inserted here.
  int active_input_index_ = 0;

  // Unowned input textfields ordered from the first to the last digit.
  std::vector<views::Textfield*> input_fields_;

  DISALLOW_COPY_AND_ASSIGN(AccessCodeInput);
};

ParentAccessView::TestApi::TestApi(ParentAccessView* view) : view_(view) {
  DCHECK(view_);
}

ParentAccessView::TestApi::~TestApi() = default;

LoginButton* ParentAccessView::TestApi::back_button() const {
  return view_->back_button_;
}

views::Label* ParentAccessView::TestApi::title_label() const {
  return view_->title_label_;
}

views::Label* ParentAccessView::TestApi::description_label() const {
  return view_->description_label_;
}

views::View* ParentAccessView::TestApi::access_code_view() const {
  return view_->access_code_view_;
}

views::LabelButton* ParentAccessView::TestApi::help_button() const {
  return view_->help_button_;
}

ArrowButtonView* ParentAccessView::TestApi::submit_button() const {
  return view_->submit_button_;
}

LoginPinView* ParentAccessView::TestApi::pin_keyboard_view() const {
  return view_->pin_keyboard_view_;
}

ParentAccessView::State ParentAccessView::TestApi::state() const {
  return view_->state_;
}

ParentAccessView::Callbacks::Callbacks() = default;

ParentAccessView::Callbacks::Callbacks(const Callbacks& other) = default;

ParentAccessView::Callbacks::~Callbacks() = default;

ParentAccessView::ParentAccessView(const AccountId& account_id,
                                   const Callbacks& callbacks,
                                   ParentAccessRequestReason reason)
    : callbacks_(callbacks), account_id_(account_id) {
  DCHECK(callbacks.on_finished);

  // Main view contains all other views aligned vertically and centered.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      gfx::Insets(kParentAccessViewVerticalInsetDp,
                  kParentAccessViewHorizontalInsetDp),
      0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  views::BoxLayout* main_layout = SetLayoutManager(std::move(layout));

  SetPreferredSize(GetParentAccessViewSize());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const int child_view_width =
      kParentAccessViewWidthDp - 2 * kParentAccessViewHorizontalInsetDp;

  // Header view contains back button that is aligned to its start.
  auto header_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), 0);
  header_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  auto* header = new NonAccessibleView();
  header->SetPreferredSize(gfx::Size(child_view_width, 0));
  header->SetLayoutManager(std::move(header_layout));
  AddChildView(header);

  back_button_ = new LoginButton(this);
  back_button_->SetPreferredSize(gfx::Size(kArrowSizeDp, kArrowSizeDp));
  back_button_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  back_button_->SetImage(views::Button::STATE_NORMAL,
                         gfx::CreateVectorIcon(kLockScreenArrowBackIcon,
                                               kArrowSizeDp, SK_ColorWHITE));
  back_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  back_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_BACK_BUTTON_ACCESSIBLE_NAME));
  back_button_->SetFocusBehavior(FocusBehavior::ALWAYS);

  header->AddChildView(back_button_);

  // Main view icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetPreferredSize(gfx::Size(kLockIconSizeDp, kLockIconSizeDp));
  icon->SetImage(gfx::CreateVectorIcon(kParentAccessLockIcon, SK_ColorWHITE));
  AddChildView(icon);

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
    label->SetFocusBehavior(FocusBehavior::ALWAYS);
  };

  // Main view title.
  title_label_ = new views::Label(GetTitle(reason), views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  title_label_->SetFontList(gfx::FontList().Derive(
      kTitleFontSizeDeltaDp, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  decorate_label(title_label_);
  AddChildView(title_label_);

  add_spacer(kTitleToDescriptionDistanceDp);

  // Main view description.
  // TODO(crbug.com/970223): Add learn more link after description.
  description_label_ =
      new views::Label(GetDescription(reason), views::style::CONTEXT_LABEL,
                       views::style::STYLE_PRIMARY);
  description_label_->SetMultiLine(true);
  description_label_->SetLineHeight(kDescriptionTextLineHeightDp);
  description_label_->SetFontList(
      gfx::FontList().Derive(kDescriptionFontSizeDeltaDp, gfx::Font::NORMAL,
                             gfx::Font::Weight::NORMAL));
  decorate_label(description_label_);
  AddChildView(description_label_);

  add_spacer(kDescriptionToAccessCodeDistanceDp);

  // Access code input view.
  access_code_view_ =
      new AccessCodeInput(kParentAccessCodePinLength,
                          base::BindRepeating(&ParentAccessView::OnInputChange,
                                              base::Unretained(this)),
                          base::BindRepeating(&ParentAccessView::SubmitCode,
                                              base::Unretained(this)));
  access_code_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  AddChildView(access_code_view_);

  add_spacer(kAccessCodeToPinKeyboardDistanceDp);

  // Pin keyboard.
  pin_keyboard_view_ = new LoginPinView(
      LoginPinView::Style::kNumeric,
      base::BindRepeating(&AccessCodeInput::InsertDigit,
                          base::Unretained(access_code_view_)),
      base::BindRepeating(&AccessCodeInput::Backspace,
                          base::Unretained(access_code_view_)));
  // Backspace key is always enabled and |access_code_| field handles it.
  pin_keyboard_view_->OnPasswordTextChanged(false);
  AddChildView(pin_keyboard_view_);

  // Vertical spacer to consume height remaining in the view after all children
  // are accounted for.
  pin_keyboard_to_footer_spacer_ = new NonAccessibleView();
  pin_keyboard_to_footer_spacer_->SetPreferredSize(
      GetPinKeyboardToFooterSpacerSize());
  AddChildView(pin_keyboard_to_footer_spacer_);
  main_layout->SetFlexForView(pin_keyboard_to_footer_spacer_, 1);

  // Footer view contains help text button aligned to its start, submit
  // button aligned to its end and spacer view in between.
  auto* footer = new NonAccessibleView();
  footer->SetPreferredSize(gfx::Size(child_view_width, kArrowButtonSizeDp));
  auto* bottom_layout =
      footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(), 0));
  AddChildView(footer);

  help_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_HELP));
  help_button_->SetPaintToLayer();
  help_button_->layer()->SetFillsBoundsOpaquely(false);
  help_button_->SetTextSubpixelRenderingEnabled(false);
  help_button_->SetTextColor(views::Button::STATE_NORMAL, kTextColor);
  help_button_->SetTextColor(views::Button::STATE_HOVERED, kTextColor);
  help_button_->SetTextColor(views::Button::STATE_PRESSED, kTextColor);
  help_button_->SetFocusBehavior(FocusBehavior::ALWAYS);

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

  // Pin keyboard is only shown in tablet mode.
  pin_keyboard_view_->SetVisible(IsTabletMode());

  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

ParentAccessView::~ParentAccessView() = default;

void ParentAccessView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  SkColor color = gfx::kGoogleGrey900;
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    SkColor extracted_color =
        Shell::Get()->wallpaper_controller()->GetProminentColor(
            color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                      color_utils::SaturationRange::MUTED));
    if (extracted_color != kInvalidWallpaperColor &&
        extracted_color != SK_ColorTRANSPARENT) {
      color = extracted_color;
    }
  }

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color);
  canvas->DrawRoundRect(GetContentsBounds(),
                        kParentAccessViewRoundedCornerRadiusDp, flags);
}

void ParentAccessView::RequestFocus() {
  access_code_view_->RequestFocus();
}

gfx::Size ParentAccessView::CalculatePreferredSize() const {
  return GetParentAccessViewSize();
}

ui::ModalType ParentAccessView::GetModalType() const {
  // MODAL_TYPE_SYSTEM is used to get a semi-transparent background behind the
  // parent access view, when it is used directly on a widget. The overlay
  // consumes all the inputs from the user, so that they can only interact with
  // the parent access view while it is visible.
  return ui::MODAL_TYPE_SYSTEM;
}

views::View* ParentAccessView::GetInitiallyFocusedView() {
  return access_code_view_;
}

void ParentAccessView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == back_button_) {
    callbacks_.on_finished.Run(false);
  } else if (sender == help_button_) {
    // TODO(agawronska): Implement help view.
    NOTIMPLEMENTED();
  } else if (sender == submit_button_) {
    SubmitCode();
  }
}

void ParentAccessView::OnTabletModeStarted() {
  VLOG(1) << "Showing PIN keyboard in ParentAccessView";
  pin_keyboard_view_->SetVisible(true);
  // This will trigger ChildPreferredSizeChanged in parent view and Layout() in
  // view. As the result whole hierarchy will go through re-layout.
  UpdatePreferredSize();
}

void ParentAccessView::OnTabletModeEnded() {
  VLOG(1) << "Hiding PIN keyboard in ParentAccessView";
  DCHECK(pin_keyboard_view_);
  pin_keyboard_view_->SetVisible(false);
  // This will trigger ChildPreferredSizeChanged in parent view and Layout() in
  // view. As the result whole hierarchy will go through re-layout.
  UpdatePreferredSize();
}

void ParentAccessView::OnTabletControllerDestroyed() {
  tablet_mode_observer_.RemoveAll();
}

void ParentAccessView::SubmitCode() {
  base::Optional<std::string> code = access_code_view_->GetCode();
  DCHECK(code.has_value());

  bool result =
      Shell::Get()->login_screen_controller()->ValidateParentAccessCode(
          account_id_, *code);

  if (result) {
    VLOG(1) << "Parent access code successfully validated";
    callbacks_.on_finished.Run(true);
    return;
  }

  VLOG(1) << "Invalid parent access code entered";
  UpdateState(State::kError);
}

void ParentAccessView::UpdateState(State state) {
  if (state_ == state)
    return;

  state_ = state;
  switch (state_) {
    case State::kNormal: {
      access_code_view_->SetInputColor(kTextColor);
      title_label_->SetEnabledColor(kTextColor);
      title_label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_TITLE));
      return;
    }
    case State::kError: {
      access_code_view_->SetInputColor(kErrorColor);
      title_label_->SetEnabledColor(kErrorColor);
      title_label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_TITLE_ERROR));
      return;
    }
  }
}

void ParentAccessView::UpdatePreferredSize() {
  pin_keyboard_to_footer_spacer_->SetPreferredSize(
      GetPinKeyboardToFooterSpacerSize());
  SetPreferredSize(CalculatePreferredSize());
}

void ParentAccessView::OnInputChange(bool complete) {
  if (state_ == State::kError)
    UpdateState(State::kNormal);

  submit_button_->SetEnabled(complete);
}

void ParentAccessView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_PARENT_ACCESS_DIALOG_NAME));
}

}  // namespace ash
