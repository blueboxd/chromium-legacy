// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kPanelPositionButtonPadding = 16;
constexpr int kPanelPositionButtonSize = 36;
constexpr int kSeparatorHeight = 16;

std::unique_ptr<views::Separator> CreateSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparator,
      AshColorProvider::AshColorMode::kDark));
  separator->SetPreferredHeight(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      separator_spacing - kUnifiedTopShortcutSpacing, 0, separator_spacing, 0));
  return separator;
}

std::unique_ptr<views::View> CreateButtonRowContainer() {
  auto button_container = std::make_unique<views::View>();
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kPanelPositionButtonPadding, kPanelPositionButtonPadding,
                  kPanelPositionButtonPadding),
      kPanelPositionButtonPadding));
  return button_container;
}

}  // namespace

FloatingAccessibilityBubbleView::FloatingAccessibilityBubbleView(
    const TrayBubbleView::InitParams& init_params)
    : TrayBubbleView(init_params) {}

FloatingAccessibilityBubbleView::~FloatingAccessibilityBubbleView() = default;

bool FloatingAccessibilityBubbleView::IsAnchoredToStatusArea() const {
  return false;
}

const char* FloatingAccessibilityBubbleView::GetClassName() const {
  return "FloatingAccessibilityBubbleView";
}

FloatingAccessibilityView::FloatingAccessibilityView(Delegate* delegate)
    : delegate_(delegate) {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  SetLayoutManager(std::move(layout));

  // TODO(crbug.com/1061068): Add buttons view that represents enabled features.

  std::unique_ptr<views::View> tray_button_container =
      CreateButtonRowContainer();
  a11y_tray_button_ =
      tray_button_container->AddChildView(std::make_unique<FloatingMenuButton>(
          this, kUnifiedMenuAccessibilityIcon,
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY,
          /*flip_for_rtl*/ true, kTrayItemSize));

  std::unique_ptr<views::View> position_button_container =
      CreateButtonRowContainer();
  position_button_ = position_button_container->AddChildView(
      std::make_unique<FloatingMenuButton>(
          this, kAutoclickPositionBottomLeftIcon,
          IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION, /*flip_for_rtl*/ false,
          kPanelPositionButtonSize, false));

  AddChildView(std::move(tray_button_container));
  AddChildView(CreateSeparator());
  AddChildView(std::move(position_button_container));

  // Set view IDs for testing.
  position_button_->SetId(static_cast<int>(ButtonId::kPosition));
  a11y_tray_button_->SetId(static_cast<int>(ButtonId::kSettingsList));
}

FloatingAccessibilityView::~FloatingAccessibilityView() {}

void FloatingAccessibilityView::SetMenuPosition(FloatingMenuPosition position) {
  switch (position) {
    case FloatingMenuPosition::kBottomRight:
      position_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      return;
    case FloatingMenuPosition::kBottomLeft:
      position_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      return;
    case FloatingMenuPosition::kTopLeft:
      position_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      return;
    case FloatingMenuPosition::kTopRight:
      position_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      return;
    case FloatingMenuPosition::kSystemDefault:
      position_button_->SetVectorIcon(base::i18n::IsRTL()
                                          ? kAutoclickPositionBottomLeftIcon
                                          : kAutoclickPositionBottomRightIcon);
      return;
  }
}

void FloatingAccessibilityView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == a11y_tray_button_) {
    delegate_->OnDetailedMenuEnabled(!a11y_tray_button_->IsToggled());
    a11y_tray_button_->SetToggled(!a11y_tray_button_->IsToggled());
    return;
  }

  if (sender == position_button_) {
    FloatingMenuPosition new_position;
    // Rotate clockwise throughout the screen positions.
    switch (
        Shell::Get()->accessibility_controller()->GetFloatingMenuPosition()) {
      case FloatingMenuPosition::kBottomRight:
        new_position = FloatingMenuPosition::kBottomLeft;
        break;
      case FloatingMenuPosition::kBottomLeft:
        new_position = FloatingMenuPosition::kTopLeft;
        break;
      case FloatingMenuPosition::kTopLeft:
        new_position = FloatingMenuPosition::kTopRight;
        break;
      case FloatingMenuPosition::kTopRight:
        new_position = FloatingMenuPosition::kBottomRight;
        break;
      case FloatingMenuPosition::kSystemDefault:
        new_position = base::i18n::IsRTL() ? FloatingMenuPosition::kTopLeft
                                           : FloatingMenuPosition::kBottomLeft;
        break;
    }
    Shell::Get()->accessibility_controller()->SetFloatingMenuPosition(
        new_position);
  }
  return;
}

const char* FloatingAccessibilityView::GetClassName() const {
  return "AccessiblityFloatingView";
}

}  // namespace ash
