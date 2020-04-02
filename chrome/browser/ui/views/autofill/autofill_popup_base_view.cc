// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

int AutofillPopupBaseView::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MEDIUM);
}

SkColor AutofillPopupBaseView::GetBackgroundColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownBackgroundColor);
}

SkColor AutofillPopupBaseView::GetForegroundColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownForegroundColor);
}

SkColor AutofillPopupBaseView::GetSelectedBackgroundColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownSelectedBackgroundColor);
}

SkColor AutofillPopupBaseView::GetSelectedForegroundColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownSelectedForegroundColor);
}

SkColor AutofillPopupBaseView::GetFooterBackgroundColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_BubbleFooterBackground);
}

SkColor AutofillPopupBaseView::GetSeparatorColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_MenuSeparatorColor);
}

SkColor AutofillPopupBaseView::GetWarningColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh);
}

AutofillPopupBaseView::AutofillPopupBaseView(
    AutofillPopupViewDelegate* delegate,
    views::Widget* parent_widget)
    : delegate_(delegate), parent_widget_(parent_widget) {}

AutofillPopupBaseView::~AutofillPopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();

    RemoveWidgetObservers();
  }
}

void AutofillPopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    // On Mac Cocoa browser, |parent_widget_| is null (the parent is not a
    // views::Widget).
    // TODO(crbug.com/826862): Remove |parent_widget_|.
    if (parent_widget_)
      parent_widget_->AddObserver(this);

    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = parent_widget_ ? parent_widget_->GetNativeView()
                                   : delegate_->container_view();
    // Ensure the bubble border is not painted on an opaque background.
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    widget->Init(std::move(params));
    widget->AddObserver(this);

    // No animation for popup appearance (too distracting).
    widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

    show_time_ = base::Time::Now();
  }

  GetWidget()->GetRootView()->SetBorder(CreateBorder());
  DoUpdateBoundsAndRedrawPopup();
  GetWidget()->Show();

  // Showing the widget can change native focus (which would result in an
  // immediate hiding of the popup). Only start observing after shown.
  if (initialize_widget)
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

void AutofillPopupBaseView::DoHide() {
  // The controller is no longer valid after it hides us.
  delegate_ = NULL;

  RemoveWidgetObservers();

  if (GetWidget()) {
    // Don't call CloseNow() because some of the functions higher up the stack
    // assume the the widget is still valid after this point.
    // http://crbug.com/229224
    // NOTE: This deletes |this|.
    GetWidget()->Close();
  } else {
    delete this;
  }
}

void AutofillPopupBaseView::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  DCHECK(widget == parent_widget_ || widget == GetWidget());
  if (widget != parent_widget_)
    return;

  HideController(PopupHidingReason::kWidgetChanged);
}

void AutofillPopupBaseView::OnWidgetDestroying(views::Widget* widget) {
  // On Windows, widgets can be destroyed in any order. Regardless of which
  // widget is destroyed first, remove all observers and hide the popup.
  DCHECK(widget == parent_widget_ || widget == GetWidget());

  // Normally this happens at destruct-time or hide-time, but because it depends
  // on |parent_widget_| (which is about to go away), it needs to happen sooner
  // in this case.
  RemoveWidgetObservers();

  // Because the parent widget is about to be destroyed, we null out the weak
  // reference to it and protect against possibly accessing it during
  // destruction (e.g., by attempting to remove observers).
  parent_widget_ = nullptr;

  HideController(PopupHidingReason::kWidgetChanged);
}

void AutofillPopupBaseView::RemoveWidgetObservers() {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  GetWidget()->RemoveObserver(this);

  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void AutofillPopupBaseView::UpdateClipPath() {
  SkRect local_bounds = gfx::RectToSkRect(GetLocalBounds());
  SkScalar radius = SkIntToScalar(GetCornerRadius());
  SkPath clip_path;
  clip_path.addRoundRect(local_bounds, radius, radius);
  SetClipPath(clip_path);
}

gfx::Rect AutofillPopupBaseView::GetWindowBounds() const {
// The call to FindBrowserWithWindow will fail on Android, so we use
// platform-specific calls.
#if defined(OS_ANDROID)
  return delegate()->container_view()->GetWindowAndroid()->bounds();
#else
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      delegate()->container_view());
  if (widget)
    return widget->GetWindowBoundsInScreen();

  // If the browser is null, simply return an empty rect. The most common reason
  // to end up here is that the NativeView has been destroyed externally, which
  // can happen at any time. This happens fairly commonly on Windows (e.g., at
  // shutdown) in particular.
  return gfx::Rect();
#endif
}

void AutofillPopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = GetPreferredSize();

  // When a bubble border is shown, the contents area (inside the shadow) is
  // supposed to be aligned with input element boundaries.
  gfx::Rect element_bounds = gfx::ToEnclosingRect(delegate()->element_bounds());
  element_bounds.Inset(/*horizontal=*/0, /*vertical=*/-kElementBorderPadding);

  gfx::Rect popup_bounds = CalculatePopupBounds(
      preferred_size, GetWindowBounds(), element_bounds, delegate()->IsRTL());
  // Account for the scroll view's border so that the content has enough space.
  popup_bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);

  Layout();
  UpdateClipPath();
  SchedulePaint();
}

void AutofillPopupBaseView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (GetWidget() && GetWidget()->GetNativeView() != focused_now)
    HideController(PopupHidingReason::kFocusChanged);
}

void AutofillPopupBaseView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(aleventhal) The correct role spec-wise to use here is kMenu, however
  // as of NVDA 2018.2.1, firing a menu event with kMenu breaks left/right
  // arrow editing feedback in text field. If NVDA addresses this we should
  // consider returning to using kMenu, so that users are notified that a
  // menu popup has been shown.
  node_data->role = ax::mojom::Role::kPane;
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void AutofillPopupBaseView::HideController(PopupHidingReason reason) {
  if (delegate_)
    delegate_->Hide(reason);
  // This will eventually result in the deletion of |this|, as the delegate
  // will hide |this|. See |DoHide| above for an explanation on why the precise
  // timing of that deletion is tricky.
}

std::unique_ptr<views::Border> AutofillPopupBaseView::CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::SMALL_SHADOW,
      SK_ColorWHITE);
  border->SetCornerRadius(GetCornerRadius());
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          views::EMPHASIS_MEDIUM));
  return border;
}

gfx::NativeView AutofillPopupBaseView::container_view() {
  return delegate_->container_view();
}

}  // namespace autofill
