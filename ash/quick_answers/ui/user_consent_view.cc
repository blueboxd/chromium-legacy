// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/ui/user_consent_view.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace quick_answers {

namespace {

// Main view (or common) specs.
constexpr int kMarginDip = 10;
constexpr int kLineHeightDip = 20;
constexpr int kContentSpacingDip = 8;
constexpr gfx::Insets kMainViewInsets = {16, 12, 16, 16};
constexpr gfx::Insets kContentInsets = {0, 12, 0, 0};
constexpr SkColor kMainViewBgColor = SK_ColorWHITE;

// Assistant icon.
constexpr int kAssistantIconSizeDip = 16;

// Title text.
constexpr SkColor kTitleTextColor = gfx::kGoogleGrey900;
constexpr int kTitleFontSizeDelta = 2;

// Description text.
constexpr SkColor kDescTextColor = gfx::kGoogleGrey700;
constexpr int kDescFontSizeDelta = 1;

// Buttons common.
constexpr int kButtonSpacingDip = 8;
constexpr int kButtonBorderRadiusDip = 4;
constexpr int kButtonBorderThicknessDip = 1;
constexpr gfx::Insets kButtonBarInsets = {8, 0, 0, 0};
constexpr gfx::Insets kButtonInsets = {6, 16, 6, 16};
constexpr int kButtonFontSizeDelta = 1;

// Manage-Settings button.
constexpr SkColor kSettingsButtonBorderColor = gfx::kGoogleGrey300;
constexpr SkColor kSettingsButtonTextColor = gfx::kGoogleBlue600;

// Grant-Consent button.
constexpr SkColor kConsentButtonBgColor = gfx::kGoogleBlue600;
constexpr SkColor kConsentButtonTextColor = gfx::kGoogleGrey200;

// Dogfood button.
constexpr int kDogfoodButtonMarginDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr SkColor kDogfoodButtonColor = gfx::kGoogleGrey500;

// Create and return a simple label with provided specs.
std::unique_ptr<views::Label> CreateLabel(const base::string16& text,
                                          const SkColor color,
                                          int font_size_delta) {
  auto label = std::make_unique<views::Label>(text);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(color);
  label->SetLineHeight(kLineHeightDip);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(font_size_delta));
  return label;
}

// views::LabelButton with custom line-height, color and font-list for the
// underlying label.
class CustomizedLabelButton : public views::LabelButton {
 public:
  CustomizedLabelButton(views::ButtonListener* listener,
                        const base::string16& text,
                        const SkColor color)
      : LabelButton(listener, text) {
    SetEnabledTextColors(color);
    label()->SetLineHeight(kLineHeightDip);
    label()->SetFontList(views::Label::GetDefaultFontList()
                             .DeriveWithSizeDelta(kButtonFontSizeDelta)
                             .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  }

  // Disallow copy and assign.
  CustomizedLabelButton(const CustomizedLabelButton&) = delete;
  CustomizedLabelButton& operator=(const CustomizedLabelButton&) = delete;

  ~CustomizedLabelButton() override = default;

  // views::View:
  const char* GetClassName() const override { return "CustomizedLabelButton"; }
};

}  // namespace

// UserConsentViewPreTargetHandler ---------------------------------------------

// TODO(siabhijeet): Reuse pre-target handler for QuickAnswersView.
class UserConsentViewPreTargetHandler : public ui::EventHandler {
 public:
  explicit UserConsentViewPreTargetHandler(UserConsentView* view)
      : view_(view) {
    Shell::Get()->AddPreTargetHandler(this);
  }

  ~UserConsentViewPreTargetHandler() override {
    Shell::Get()->RemovePreTargetHandler(this);
  }

  UserConsentViewPreTargetHandler(const UserConsentViewPreTargetHandler&) =
      delete;
  UserConsentViewPreTargetHandler& operator=(
      const UserConsentViewPreTargetHandler&) = delete;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (!event->IsLocatedEvent())
      return;
    auto* located_event = event->AsLocatedEvent();
    auto location = located_event->target()->GetScreenLocation(*located_event);
    if (view_->GetBoundsInScreen().Contains(location)) {
      DoDispatchEvent(view_, located_event);
      event->StopPropagation();
      auto* tooltip_manager = view_->GetWidget()->GetTooltipManager();
      if (tooltip_manager)
        tooltip_manager->UpdateTooltip();
    }
  }

 private:
  // TODO(siabhijeet): Investigate using SendEventsToSink() instead.
  bool DoDispatchEvent(views::View* view, ui::LocatedEvent* event) {
    if (event->handled())
      return true;

    // Convert |event| to local coordinates of |view|.
    gfx::Point location = event->target()->GetScreenLocation(*event);
    views::View::ConvertPointFromScreen(view, &location);
    event->set_location(location);
    ui::Event::DispatcherApi(event).set_target(view);

    // Process event and dispatch on children recursively.
    view->OnEvent(event);
    for (auto* child : view->children()) {
      if (DoDispatchEvent(child, event))
        return true;
    }
    return false;
  }

  // Associated view handled by this class.
  UserConsentView* const view_;
};

// UserConsentView -------------------------------------------------------------

UserConsentView::UserConsentView(const gfx::Rect& anchor_view_bounds,
                                 QuickAnswersUiController* ui_controller)
    : anchor_view_bounds_(anchor_view_bounds),
      event_handler_(std::make_unique<UserConsentViewPreTargetHandler>(this)),
      ui_controller_(ui_controller) {
  InitLayout();
  InitWidget();

  // Allow tooltips to be shown despite menu-controller owning capture.
  GetWidget()->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
}

UserConsentView::~UserConsentView() = default;

const char* UserConsentView::GetClassName() const {
  return "UserConsentView";
}

gfx::Size UserConsentView::CalculatePreferredSize() const {
  // View should match width of the anchor.
  auto width = anchor_view_bounds_.width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void UserConsentView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender == consent_button_) {
    ui_controller_->OnConsentGrantedButtonPressed();
    return;
  }
  if (sender == settings_button_) {
    ui_controller_->OnManageSettingsButtonPressed();
    return;
  }
  if (sender == dogfood_button_) {
    ui_controller_->OnDogfoodButtonPressed();
    return;
  }
}

void UserConsentView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateWidgetBounds();
}

void UserConsentView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBackground(views::CreateSolidBackground(kMainViewBgColor));

  // Main-view Layout.
  main_view_ = AddChildView(std::make_unique<views::View>());
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMainViewInsets));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Assistant icon.
  auto* assistant_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  assistant_icon->SetBorder(views::CreateEmptyBorder(
      (kLineHeightDip - kAssistantIconSizeDip) / 2, 0, 0, 0));
  assistant_icon->SetImage(gfx::CreateVectorIcon(
      kAssistantIcon, kAssistantIconSizeDip, gfx::kPlaceholderColor));

  // Content.
  InitContent();

  // Add dogfood icon, if in dogfood.
  if (chromeos::features::IsQuickAnswersDogfood())
    AddDogfoodButton();
}

void UserConsentView::InitContent() {
  // Layout.
  content_ = main_view_->AddChildView(std::make_unique<views::View>());
  content_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContentInsets,
      kContentSpacingDip));

  // Title.
  content_->AddChildView(
      CreateLabel(l10n_util::GetStringUTF16(
                      IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_TITLE_TEXT),
                  kTitleTextColor, kTitleFontSizeDelta));

  // Description.
  auto* desc = content_->AddChildView(
      CreateLabel(l10n_util::GetStringUTF16(
                      IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT),
                  kDescTextColor, kDescFontSizeDelta));
  desc->SetMultiLine(true);
  // BoxLayout does not necessarily size the height of multi-line labels
  // properly (crbug/682266). The label is thus explicitly sized to the width
  // (and height) it would need to be for the UserConsentView to be the same
  // width as the anchor, so its preferred size will be calculated correctly.
  int desc_desired_width = anchor_view_bounds_.width() -
                           kMainViewInsets.width() - kContentInsets.width() -
                           kAssistantIconSizeDip;
  desc->SizeToFit(desc_desired_width);

  // Button bar.
  InitButtonBar();
}

void UserConsentView::InitButtonBar() {
  // Layout.
  auto* button_bar = content_->AddChildView(std::make_unique<views::View>());
  auto* layout =
      button_bar->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kButtonBarInsets,
          kButtonSpacingDip));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  // Manage-Settings button.
  auto settings_button = std::make_unique<CustomizedLabelButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_MANAGE_SETTINGS_BUTTON),
      kSettingsButtonTextColor);
  settings_button->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(kButtonBorderThicknessDip,
                                     kButtonBorderRadiusDip,
                                     kSettingsButtonBorderColor),
      kButtonInsets));
  settings_button_ = button_bar->AddChildView(std::move(settings_button));

  // Grant-Consent button.
  auto consent_button = std::make_unique<CustomizedLabelButton>(
      this,
      l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_USER_CONSENT_VIEW_GRANT_CONSENT_BUTTON),
      kConsentButtonTextColor);
  consent_button->SetBackground(views::CreateRoundedRectBackground(
      kConsentButtonBgColor, kButtonBorderRadiusDip));
  consent_button->SetBorder(views::CreateEmptyBorder(kButtonInsets));
  consent_button_ = button_bar->AddChildView(std::move(consent_button));
}

void UserConsentView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.context = Shell::Get()->GetRootWindowForNewWindows();
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateWidgetBounds();
}

void UserConsentView::AddDogfoodButton() {
  auto* dogfood_view = AddChildView(std::make_unique<views::View>());
  auto* layout =
      dogfood_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kDogfoodButtonMarginDip)));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  auto dogfood_button = std::make_unique<views::ImageButton>(/*listener=*/this);
  dogfood_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kDogfoodIcon, kDogfoodButtonSizeDip,
                            kDogfoodButtonColor));
  dogfood_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_ANSWERS_DOGFOOD_BUTTON_TOOLTIP_TEXT));
  dogfood_button_ = dogfood_view->AddChildView(std::move(dogfood_button));
}

void UserConsentView::UpdateWidgetBounds() {
  const gfx::Size size = GetPreferredSize();
  int x = anchor_view_bounds_.x();
  int y = anchor_view_bounds_.y() - size.height() - kMarginDip;
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds_)
              .bounds()
              .y()) {
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }
  GetWidget()->SetBounds({{x, y}, size});
}

}  // namespace quick_answers
}  // namespace ash
