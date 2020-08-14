// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/auth_dialog_contents_view.h"

#include <memory>
#include <utility>

#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

enum class ButtonId {
  kMoreOptions,
  kCancel,
};

// TODO(b/164195709): Move these strings to a grd file.
const char kTitle[] = "Verify it's you";
const char kFingerprintPrompt[] = "Authenticate with fingerprint";
// If fingerprint option is available, password input field will be hidden
// until the user taps the MoreOptions button.
const char kMoreOptionsButtonText[] = "More options";
const char kCancelButtonText[] = "Cancel";

const int kContainerPreferredWidth = 512;
const int kTopVerticalSpacing = 24;
const int kVerticalSpacingBetweenTitleAndPrompt = 16;
const int kVerticalSpacingBetweenPasswordAndPINKeyboard = 16;
const int kBottomVerticalSpacing = 20;
const int kButtonSpacing = 8;

const int kTitleFontSize = 14;
const int kPromptFontSize = 12;

constexpr int kFingerprintIconSizeDp = 28;
constexpr int kFingerprintIconTopSpacingDp = 20;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;

}  // namespace

// Consists of fingerprint icon view and a label.
class AuthDialogContentsView::FingerprintView : public views::View {
 public:
  FingerprintView() {
    SetBorder(views::CreateEmptyBorder(kFingerprintIconTopSpacingDp, 0, 0, 0));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kSpacingBetweenFingerprintIconAndLabelDp));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    icon_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
        gfx::Size(kFingerprintIconSizeDp, kFingerprintIconSizeDp),
        0 /*corner_radius*/));
    icon_->SetImage(gfx::CreateVectorIcon(
        kLockScreenFingerprintIcon, kFingerprintIconSizeDp, SK_ColorDKGRAY));

    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetEnabledColor(SK_ColorDKGRAY);
    label_->SetMultiLine(true);
    label_->SetText(base::UTF8ToUTF16(kFingerprintPrompt));

    SetVisible(true);
  }
  FingerprintView(const FingerprintView&) = delete;
  FingerprintView& operator=(const FingerprintView&) = delete;
  ~FingerprintView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = views::View::CalculatePreferredSize();
    size.set_width(kFingerprintViewWidthDp);
    return size;
  }

 private:
  views::Label* label_ = nullptr;
  AnimatedRoundedImageView* icon_ = nullptr;
};

AuthDialogContentsView::AuthDialogContentsView(uint32_t auth_methods)
    : auth_methods_(auth_methods) {
  DCHECK(auth_methods_ & kAuthPassword);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = AddChildView(std::make_unique<NonAccessibleView>());
  container_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  main_layout_ =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  main_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  main_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddVerticalSpacing(kTopVerticalSpacing);
  AddTitleView();
  AddVerticalSpacing(kVerticalSpacingBetweenTitleAndPrompt);
  // TODO(b/156258540): Add proper spacing once all elements are determined.
  AddPasswordView();
  AddPinView();
  AddVerticalSpacing(kVerticalSpacingBetweenPasswordAndPINKeyboard);

  if (auth_methods_ & kAuthFingerprint) {
    fingerprint_view_ =
        container_->AddChildView(std::make_unique<FingerprintView>());
  }

  AddActionButtonsView();
  AddVerticalSpacing(kBottomVerticalSpacing);

  // Deferred because it needs the pin_view_ pointer.
  InitPasswordView();
}

AuthDialogContentsView::~AuthDialogContentsView() = default;

void AuthDialogContentsView::AddTitleView() {
  title_ = container_->AddChildView(std::make_unique<views::Label>());
  title_->SetEnabledColor(SK_ColorBLACK);
  title_->SetSubpixelRenderingEnabled(false);
  title_->SetAutoColorReadabilityEnabled(false);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  title_->SetFontList(base_font_list.Derive(
      kTitleFontSize, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  title_->SetText(base::UTF8ToUTF16(kTitle));
  title_->SetMaximumWidth(kContainerPreferredWidth);
  title_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

void AuthDialogContentsView::AddPromptView() {
  prompt_ = container_->AddChildView(std::make_unique<views::Label>());
  prompt_->SetEnabledColor(SK_ColorBLACK);
  prompt_->SetSubpixelRenderingEnabled(false);
  prompt_->SetAutoColorReadabilityEnabled(false);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  prompt_->SetFontList(base_font_list.Derive(kPromptFontSize,
                                             gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::NORMAL));
  // TODO(b/156258540): Use a different prompt if the board has no fingerprint
  // sensor.
  prompt_->SetText(base::UTF8ToUTF16(kFingerprintPrompt));
  prompt_->SetMaximumWidth(kContainerPreferredWidth);
  prompt_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

void AuthDialogContentsView::AddPasswordView() {
  password_view_ = container_->AddChildView(
      std::make_unique<LoginPasswordView>(CreateInSessionAuthPalette()));

  password_view_->SetPaintToLayer();
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->SetDisplayPasswordButtonVisible(true);
  password_view_->SetEnabled(true);
  password_view_->SetEnabledOnEmptyPassword(false);
  password_view_->SetFocusEnabledForChildViews(true);
  password_view_->SetVisible(true);

  password_view_->SetPlaceholderText(
      (auth_methods_ & kAuthPin)
          ? l10n_util::GetStringUTF16(
                IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER)
          : l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
}

void AuthDialogContentsView::AddPinView() {
  pin_view_ = container_->AddChildView(std::make_unique<LoginPinView>(
      LoginPinView::Style::kAlphanumeric, CreateInSessionAuthPalette(),
      base::BindRepeating(&LoginPasswordView::InsertNumber,
                          base::Unretained(password_view_)),
      base::BindRepeating(&LoginPasswordView::Backspace,
                          base::Unretained(password_view_)),
      base::BindRepeating(&LoginPasswordView::SubmitPassword,
                          base::Unretained(password_view_))));
  pin_view_->SetVisible(auth_methods_ & kAuthPin);
}

void AuthDialogContentsView::InitPasswordView() {
  password_view_->Init(
      base::BindRepeating(&AuthDialogContentsView::OnAuthSubmit,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinView::OnPasswordTextChanged,
                          base::Unretained(pin_view_)),
      base::DoNothing(), base::DoNothing());
}

void AuthDialogContentsView::AddVerticalSpacing(int height) {
  auto* spacing =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kContainerPreferredWidth, height));
}

void AuthDialogContentsView::AddActionButtonsView() {
  action_view_container_ =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  auto* buttons_layout = action_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  buttons_layout->set_between_child_spacing(kButtonSpacing);

  more_options_button_ = AddButton(kMoreOptionsButtonText,
                                   static_cast<int>(ButtonId::kMoreOptions),
                                   action_view_container_);
  cancel_button_ =
      AddButton(kCancelButtonText, static_cast<int>(ButtonId::kCancel),
                action_view_container_);
}

void AuthDialogContentsView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == cancel_button_) {
    // Cancel() deletes |this|.
    InSessionAuthDialogController::Get()->Cancel();
  }

  // TODO(b/156258540): Enable more options button when we have both fingerprint
  // view and password input view.
}

views::LabelButton* AuthDialogContentsView::AddButton(const std::string& text,
                                                      int id,
                                                      views::View* container) {
  // Creates a button with |text|.
  auto button =
      std::make_unique<views::MdTextButton>(this, base::ASCIIToUTF16(text));
  button->SetID(id);

  views::LabelButton* view = button.get();
  container->AddChildView(
      login_views_utils::WrapViewForPreferredSize(std::move(button)));
  return view;
}

void AuthDialogContentsView::OnAuthSubmit(const base::string16& password) {
  InSessionAuthDialogController::Get()->AuthenticateUserWithPasswordOrPin(
      base::UTF16ToUTF8(password),
      base::BindOnce(&AuthDialogContentsView::OnAuthComplete,
                     weak_factory_.GetWeakPtr()));
}

// TODO(b/156258540): Clear password/PIN if auth failed and retry is allowed.
void AuthDialogContentsView::OnAuthComplete(base::Optional<bool> success) {}

}  // namespace ash
