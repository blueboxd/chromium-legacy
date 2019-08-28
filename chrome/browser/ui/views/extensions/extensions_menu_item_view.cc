// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int EXTENSION_CONTEXT_MENU = 13;
}  // namespace

ExtensionsMenuItemView::ExtensionsMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller)
    : primary_action_button_(
          new ExtensionsMenuButton(browser, this, controller.get())),
      controller_(std::move(controller)) {
  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      nullptr, controller_.get());

  views::FlexLayout* layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true);

  AddChildView(primary_action_button_);
  primary_action_button_->SetProperty(
      views::kFlexBehaviorKey, views::FlexSpecification::ForSizeRule(
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded));

  const SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);

  // TODO(pbos): There's complicated configuration code in place since menus
  // can't be triggered from ImageButtons. When MenuRunner::RunMenuAt accepts
  // views::Buttons, turn this into a views::ImageButton and use
  // image_button_factory.h methods to configure it.
  auto context_menu_button =
      std::make_unique<views::MenuButton>(base::string16(), this);
  context_menu_button->SetID(EXTENSION_CONTEXT_MENU);
  context_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP));

  context_menu_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kBrowserToolsIcon, kSecondaryIconSizeDp,
                            icon_color));

  context_menu_button->set_ink_drop_base_color(icon_color);
  context_menu_button->SetBorder(
      views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON)));

  context_menu_button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  context_menu_button->set_has_ink_drop_action_on_click(true);

  context_menu_button_ = context_menu_button.get();
  AddChildView(std::move(context_menu_button));
}

ExtensionsMenuItemView::~ExtensionsMenuItemView() = default;

void ExtensionsMenuItemView::OnMenuButtonClicked(views::Button* source,
                                                 const gfx::Point& point,
                                                 const ui::Event* event) {
  DCHECK_EQ(source->GetID(), EXTENSION_CONTEXT_MENU);

  // TODO(crbug.com/998298): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      source, point, ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void ExtensionsMenuItemView::UpdatePinButton() {
  primary_action_button_->UpdatePinButton();
}

ExtensionsMenuButton*
ExtensionsMenuItemView::primary_action_button_for_testing() {
  return primary_action_button_;
}
