// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"

#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "content/public/browser/notification_source.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/mouse_constants.h"

using views::LabelButtonBorder;

namespace {

// Toolbar action buttons have no insets because the badges are drawn right at
// the edge of the view's area. Other badding (such as centering the icon) is
// handled directly by the Image.
const int kBorderInset = 0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView::Delegate

bool ToolbarActionView::Delegate::CanShowIconInToolbar() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView

const char ToolbarActionView::kClassName[] = "ToolbarActionView";

ToolbarActionView::ToolbarActionView(
    ToolbarActionViewController* view_controller,
    ToolbarActionView::Delegate* delegate)
    : MenuButton(base::string16(), this),
      view_controller_(view_controller),
      delegate_(delegate) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetID(VIEW_ID_BROWSER_ACTION);
  view_controller_->SetDelegate(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  set_drag_controller(delegate_);

  set_context_menu_controller(this);

  // If the button is within a menu, we need to make it focusable in order to
  // have it accessible via keyboard navigation.
  if (delegate_->ShownInsideMenu())
    SetFocusBehavior(FocusBehavior::ALWAYS);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  UpdateState();
}

ToolbarActionView::~ToolbarActionView() {
  view_controller_->SetDelegate(nullptr);
}

const char* ToolbarActionView::GetClassName() const {
  return kClassName;
}

void ToolbarActionView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // TODO(pbos): Consolidate with ToolbarButton::OnBoundsChanged.
  SetToolbarButtonHighlightPath(this, gfx::Insets());

  MenuButton::OnBoundsChanged(previous_bounds);
}

gfx::Rect ToolbarActionView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetToolbarInkDropInsets(this, gfx::Insets()));
  return bounds;
}

void ToolbarActionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::MenuButton::GetAccessibleNodeData(node_data);
  node_data->role = delegate_->ShownInsideMenu() ? ax::mojom::Role::kMenuItem
                                                 : ax::mojom::Role::kButton;
}

std::unique_ptr<LabelButtonBorder> ToolbarActionView::CreateDefaultBorder()
    const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  border->set_insets(
      gfx::Insets(kBorderInset, kBorderInset, kBorderInset, kBorderInset));
  return border;
}

bool ToolbarActionView::IsTriggerableEvent(const ui::Event& event) {
  // By default MenuButton checks the time since the menu closure, but that
  // prevents left clicks from showing the extension popup when the context menu
  // is showing.  The time check is to prevent reshowing on the same click that
  // closed the menu, when this class handles via |suppress_next_release_|, so
  // it's not necessary.  Bypass it by calling IsTriggerableEventType() instead
  // of IsTriggerableEvent().
  return button_controller()->IsTriggerableEventType(event);
}

SkColor ToolbarActionView::GetInkDropBaseColor() const {
  if (delegate_->ShownInsideMenu()) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
  }

  return GetToolbarInkDropBaseColor(this);
}

std::unique_ptr<views::InkDrop> ToolbarActionView::CreateInkDrop() {
  auto ink_drop = MenuButton::CreateInkDrop();
  ink_drop->SetShowHighlightOnHover(!delegate_->ShownInsideMenu());
  return ink_drop;
}

std::unique_ptr<views::InkDropHighlight>
ToolbarActionView::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

bool ToolbarActionView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_DOWN) {
    ShowContextMenuForViewImpl(this, gfx::Point(), ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return MenuButton::OnKeyPressed(event);
}

content::WebContents* ToolbarActionView::GetCurrentWebContents() const {
  return delegate_->GetCurrentWebContents();
}

void ToolbarActionView::UpdateState() {
  content::WebContents* web_contents = GetCurrentWebContents();
  SetAccessibleName(view_controller_->GetAccessibleName(web_contents));
  if (!SessionTabHelper::IdForTab(web_contents).is_valid())
    return;

  if (!view_controller_->IsEnabled(web_contents) &&
      !view_controller_->DisabledClickOpensMenu()) {
    SetState(views::Button::STATE_DISABLED);
  } else if (state() == views::Button::STATE_DISABLED) {
    SetState(views::Button::STATE_NORMAL);
  }

  wants_to_run_ = view_controller_->WantsToRun(web_contents);

  gfx::ImageSkia icon(
      view_controller_->GetIcon(web_contents, GetPreferredSize())
          .AsImageSkia());

  if (!icon.isNull())
    SetImage(views::Button::STATE_NORMAL, icon);

  SetTooltipText(view_controller_->GetTooltip(web_contents));

  Layout();  // We need to layout since we may have added an icon as a result.
  SchedulePaint();
}

void ToolbarActionView::OnMenuButtonClicked(views::Button* sender,
                                            const gfx::Point& point,
                                            const ui::Event* event) {
  if (!view_controller_->IsEnabled(GetCurrentWebContents())) {
    // We should only get a button pressed event with a non-enabled action if
    // the left-click behavior should open the menu.
    DCHECK(view_controller_->DisabledClickOpensMenu());
    context_menu_controller()->ShowContextMenuForView(this, point,
                                                      ui::MENU_SOURCE_NONE);
  } else {
    view_controller_->ExecuteAction(true);
  }
}

bool ToolbarActionView::IsMenuRunningForTesting() const {
  return IsMenuRunning();
}

void ToolbarActionView::OnMenuClosed() {
  menu_runner_.reset();
  menu_ = nullptr;
  view_controller_->OnContextMenuClosed();
  menu_adapter_.reset();
}

gfx::ImageSkia ToolbarActionView::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
}

gfx::Size ToolbarActionView::CalculatePreferredSize() const {
  return delegate_->GetToolbarActionSize();
}

bool ToolbarActionView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    if (view_controller()->IsShowingPopup()) {
      // Left-clicking the button should always hide the popup.  In most cases,
      // this would have happened automatically anyway due to the popup losing
      // activation, but if the popup is currently being inspected, the
      // activation loss will not automatically close it, so force-hide here.
      view_controller_->HidePopup();

      // Since we just hid the popup, don't allow the mouse release for this
      // click to re-show it.
      suppress_next_release_ = true;
    } else {
      // This event is likely to trigger the MenuButton action.
      // TODO(bruthig): The ACTION_PENDING triggering logic should be in
      // MenuButton::OnPressed() however there is a bug with the pressed state
      // logic in MenuButton. See http://crbug.com/567252.
      AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
    }
  }

  return MenuButton::OnMousePressed(event);
}

void ToolbarActionView::OnMouseReleased(const ui::MouseEvent& event) {
  // MenuButton::OnMouseReleased() may synchronously delete |this|, so writing
  // member variables after that point is unsafe.  Instead, copy the old value
  // of |suppress_next_release_| so it can be updated now.
  const bool suppress_next_release = suppress_next_release_;
  suppress_next_release_ = false;
  if (!suppress_next_release)
    MenuButton::OnMouseReleased(event);
}

void ToolbarActionView::OnGestureEvent(ui::GestureEvent* event) {
  // While the dropdown menu is showing, the button should not handle gestures.
  if (menu_)
    event->StopPropagation();
  else
    MenuButton::OnGestureEvent(event);
}

void ToolbarActionView::OnDragDone() {
  views::MenuButton::OnDragDone();

  // The mouse release that ends a drag does not generate a mouse release event,
  // so OnMouseReleased() doesn't get called.  Thus if the click that started
  // the drag set |suppress_next_release_|, it must be reset here or the next
  // mouse release after the drag will be erroneously discarded.
  suppress_next_release_ = false;

  delegate_->OnToolbarActionViewDragDone();
}

void ToolbarActionView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && !called_register_command_ && GetFocusManager()) {
    view_controller_->RegisterCommand();
    called_register_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(details);
}

views::View* ToolbarActionView::GetAsView() {
  return this;
}

views::FocusManager* ToolbarActionView::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ToolbarActionView::GetReferenceButtonForPopup() {
  // Browser actions in the overflow menu can still show popups, so we may need
  // a reference view other than this button's parent. If so, use the overflow
  // view which is a BrowserAppMenuButton.
  return GetVisible() ? this : delegate_->GetOverflowReferenceView();
}

bool ToolbarActionView::IsMenuRunning() const {
  return menu_ != nullptr;
}

bool ToolbarActionView::CanShowIconInToolbar() const {
  return delegate_->CanShowIconInToolbar();
}

void ToolbarActionView::OnPopupShown(bool by_user) {
  // If this was through direct user action, we press the menu button.
  if (by_user) {
    // GetReferenceButtonForPopup returns either |this| or
    // delegate_->GetOverflowReferenceView() which is a BrowserAppMenuButton.
    // This cast is safe because both will have a MenuButtonController.
    views::MenuButtonController* reference_view_controller =
        static_cast<views::MenuButtonController*>(
            GetReferenceButtonForPopup()->button_controller());
    pressed_lock_ = reference_view_controller->TakeLock();
  }
}

void ToolbarActionView::OnPopupClosed() {
  pressed_lock_.reset();  // Unpress the menu button if it was pressed.
}

void ToolbarActionView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (CloseActiveMenuIfNeeded())
    return;

  // Otherwise, no other menu is showing, and we can proceed normally.
  DoShowContextMenu(source_type);
}

void ToolbarActionView::DoShowContextMenu(ui::MenuSourceType source_type) {
  ui::MenuModel* context_menu_model = view_controller_->GetContextMenu();
  // It's possible the action doesn't have a context menu.
  if (!context_menu_model)
    return;

  DCHECK(
      GetVisible());  // We should never show a context menu for a hidden item.

  int run_types =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
  if (delegate_->ShownInsideMenu())
    run_types |= views::MenuRunner::IS_NESTED;

  // RunMenuAt expects a nested menu to be parented by the same widget as the
  // already visible menu, in this case the Chrome menu.
  views::Widget* parent =
      delegate_->ShownInsideMenu()
          ? delegate_->GetOverflowReferenceView()->GetWidget()
          : GetWidget();

  // Unretained() is safe here as ToolbarActionView will always outlive the
  // menu. Any action that would lead to the deletion of |this| first triggers
  // the closing of the menu through lost capture.
  menu_adapter_.reset(new views::MenuModelAdapter(
      context_menu_model, base::BindRepeating(&ToolbarActionView::OnMenuClosed,
                                              base::Unretained(this))));
  menu_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(new views::MenuRunner(menu_, run_types));

  menu_runner_->RunMenuAt(parent, button_controller(),
                          GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

bool ToolbarActionView::CloseActiveMenuIfNeeded() {
  // If this view is shown inside another menu, there's a possibility that there
  // is another context menu showing that we have to close before we can
  // activate a different menu.
  if (delegate_->ShownInsideMenu()) {
    views::MenuController* menu_controller =
        views::MenuController::GetActiveInstance();
    // If this is shown inside a menu, then there should always be an active
    // menu controller.
    DCHECK(menu_controller);
    if (menu_controller->in_nested_run()) {
      // There is another menu showing. Close the outermost menu (since we are
      // shown in the same menu, we don't want to close the whole thing).
      menu_controller->Cancel(views::MenuController::ExitType::kOutermost);
      return true;
    }
  }

  return false;
}
