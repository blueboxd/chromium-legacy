// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

const char ExtensionsMenuButton::kClassName[] = "ExtensionsMenuButton";

namespace {

constexpr int EXTENSION_CONTEXT_MENU = 13;

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
      controller_(std::move(controller)) {
  ConfigureSecondaryView();
  set_auto_compute_tooltip(false);
  controller_->SetDelegate(this);
  UpdateState();
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

const char* ExtensionsMenuButton::GetClassName() const {
  return kClassName;
}

void ExtensionsMenuButton::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  if (sender->GetID() == EXTENSION_CONTEXT_MENU) {
    RunExtensionContextMenu(ui::MENU_SOURCE_MOUSE);
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
  return menu_runner_ && menu_runner_->IsRunning();
}

void ExtensionsMenuButton::RunExtensionContextMenu(
    ui::MenuSourceType source_type) {
  ui::MenuModel* model = controller_->GetContextMenu();
  if (!model)
    return;

  // Unretained() is safe here as ExtensionsMenuButton will always outlive the
  // menu. Any action that would lead to the deletion of |this| first triggers
  // the closing of the menu through lost capture.
  menu_adapter_ = std::make_unique<views::MenuModelAdapter>(
      model, base::BindRepeating(&ExtensionsMenuButton::OnMenuClosed,
                                 base::Unretained(this)));

  menu_runner_ = std::make_unique<views::MenuRunner>(
      model, views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(),
                          context_menu_button_->button_controller(),
                          context_menu_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight, source_type);
}

void ExtensionsMenuButton::OnMenuClosed() {
  menu_runner_.reset();
  controller_->OnContextMenuClosed();
  menu_adapter_.reset();
}

void ExtensionsMenuButton::ConfigureSecondaryView() {
  views::View* container = secondary_view();
  DCHECK(container->children().empty());
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  const SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey;

  auto context_menu_button =
      std::make_unique<views::MenuButton>(base::string16(), this);
  context_menu_button->SetID(EXTENSION_CONTEXT_MENU);
  context_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP));

  context_menu_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kBrowserToolsIcon, 16, icon_color));
  context_menu_button->set_ink_drop_base_color(icon_color);
  context_menu_button->SetBorder(
      views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON)));
  context_menu_button->SizeToPreferredSize();

  context_menu_button->SetInkDropMode(InkDropMode::ON);
  context_menu_button->set_has_ink_drop_action_on_click(true);
  auto highlight_path = std::make_unique<SkPath>();
  highlight_path->addOval(
      gfx::RectToSkRect(gfx::Rect(context_menu_button->size())));
  context_menu_button->SetProperty(views::kHighlightPathKey,
                                   highlight_path.release());

  context_menu_button_ = context_menu_button.get();
  container->AddChildView(std::move(context_menu_button));
}
