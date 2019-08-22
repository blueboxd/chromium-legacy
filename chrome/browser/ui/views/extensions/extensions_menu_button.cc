// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

const char ExtensionsMenuButton::kClassName[] = "ExtensionsMenuButton";

namespace {

constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;

constexpr int kSecondaryIconSizeDp = 16;

void SetSecondaryButtonHighlightPath(views::Button* button) {
  auto highlight_path = std::make_unique<SkPath>();
  highlight_path->addOval(gfx::RectToSkRect(gfx::Rect(button->size())));
  button->SetProperty(views::kHighlightPathKey, highlight_path.release());
}

}  // namespace

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller)
    : HoverButton(this,
                  ExtensionsMenuView::CreateFixedSizeIconView(),
                  base::string16(),
                  base::string16(),
                  std::make_unique<views::View>(),
                  true,
                  true),
      browser_(browser),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      context_menu_controller_(nullptr, controller_.get()) {
  set_context_menu_controller(&context_menu_controller_);

  // Set so the extension button receives enter/exit on children to retain hover
  // status when hovering child views.
  set_notify_enter_exit_on_child(true);
  ConfigureSecondaryView();
  set_auto_compute_tooltip(false);
  controller_->SetDelegate(this);
  UpdateState();
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

void ExtensionsMenuButton::UpdatePinButton() {
  pin_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IsPinned() ? IDS_EXTENSIONS_MENU_UNPIN_BUTTON_TOOLTIP
                 : IDS_EXTENSIONS_MENU_PIN_BUTTON_TOOLTIP));
  SkColor unpinned_icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey;
  SkColor icon_color = IsPinned()
                           ? GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : unpinned_icon_color;
  views::SetImageFromVectorIcon(
      pin_button_, IsPinned() ? views::kUnpinIcon : views::kPinIcon,
      kSecondaryIconSizeDp, icon_color);
  pin_button_->SetVisible(IsPinned() || IsMouseHovered() || IsMenuRunning());
}

void ExtensionsMenuButton::OnMouseEntered(const ui::MouseEvent& event) {
  UpdatePinButton();
  // The layout manager does not get notified of visibility changes and the pin
  // buttons has not be laid out before if it was invisible.
  pin_button_->InvalidateLayout();
  views::Button::OnMouseEntered(event);
}

void ExtensionsMenuButton::OnMouseExited(const ui::MouseEvent& event) {
  UpdatePinButton();
  views::Button::OnMouseExited(event);
}

void ExtensionsMenuButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdatePinButton();
  HoverButton::OnBoundsChanged(previous_bounds);
}

const char* ExtensionsMenuButton::GetClassName() const {
  return kClassName;
}

void ExtensionsMenuButton::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  if (sender->GetID() == EXTENSION_CONTEXT_MENU) {
    context_menu_controller()->ShowContextMenuForView(
        context_menu_button_, gfx::Point(), ui::MENU_SOURCE_MOUSE);
    return;
  } else if (sender->GetID() == EXTENSION_PINNING) {
    model_->SetActionVisibility(controller_->GetId(), !IsPinned());
    return;
  }
  DCHECK_EQ(this, sender);
  controller_->ExecuteAction(true);
}

// ToolbarActionViewDelegateViews:
views::View* ExtensionsMenuButton::GetAsView() {
  return this;
}

views::FocusManager* ExtensionsMenuButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ExtensionsMenuButton::GetReferenceButtonForPopup() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->GetExtensionsButton();
}

content::WebContents* ExtensionsMenuButton::GetCurrentWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExtensionsMenuButton::UpdateState() {
  DCHECK_EQ(views::ImageView::kViewClassName, icon_view()->GetClassName());
  static_cast<views::ImageView*>(icon_view())
      ->SetImage(controller_
                     ->GetIcon(GetCurrentWebContents(),
                               icon_view()->GetPreferredSize())
                     .AsImageSkia());
  SetTitleTextWithHintRange(controller_->GetActionName(),
                            gfx::Range::InvalidRange());
  SetTooltipText(controller_->GetTooltip(GetCurrentWebContents()));
  SetEnabled(controller_->IsEnabled(GetCurrentWebContents()));
}

bool ExtensionsMenuButton::IsMenuRunning() const {
  return context_menu_controller_.IsMenuRunning();
}

void ExtensionsMenuButton::ConfigureSecondaryView() {
  views::View* container = secondary_view();
  DCHECK(container->children().empty());
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  const SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);

  auto pin_button = views::CreateVectorImageButton(this);
  pin_button->SetID(EXTENSION_PINNING);
  pin_button->set_ink_drop_base_color(icon_color);
  pin_button->SizeToPreferredSize();

  pin_button_ = pin_button.get();
  SetSecondaryButtonHighlightPath(pin_button_);
  UpdatePinButton();
  container->AddChildView(std::move(pin_button));

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
  context_menu_button->SizeToPreferredSize();

  context_menu_button->SetInkDropMode(InkDropMode::ON);
  context_menu_button->set_has_ink_drop_action_on_click(true);

  context_menu_button_ = context_menu_button.get();
  SetSecondaryButtonHighlightPath(context_menu_button_);
  container->AddChildView(std::move(context_menu_button));
}

bool ExtensionsMenuButton::IsPinned() {
  // |model_| can be null in unit tests.
  if (!model_)
    return false;
  return model_->IsActionPinned(controller_->GetId());
}
