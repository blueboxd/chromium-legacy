// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/installable_ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

ToolbarButton::ToolbarButton(views::ButtonListener* listener)
    : ToolbarButton(listener, nullptr, nullptr) {}

ToolbarButton::ToolbarButton(views::ButtonListener* listener,
                             std::unique_ptr<ui::MenuModel> model,
                             TabStripModel* tab_strip_model,
                             bool trigger_menu_on_long_press)
    : views::LabelButton(listener, base::string16(), CONTEXT_TOOLBAR_BUTTON),
      model_(std::move(model)),
      tab_strip_model_(tab_strip_model),
      trigger_menu_on_long_press_(trigger_menu_on_long_press) {
  set_has_ink_drop_action_on_click(true);
  set_context_menu_controller(this);

  if (base::FeatureList::IsEnabled(views::kInstallableInkDropFeature)) {
    installable_ink_drop_ = std::make_unique<views::InstallableInkDrop>(this);
    installable_ink_drop_->SetConfig(GetToolbarInstallableInkDropConfig(this));
  }

  SetInkDropMode(InkDropMode::ON);

  // Make sure icons are flipped by default so that back, forward, etc. follows
  // UI direction.
  EnableCanvasFlippingForRTLUI(true);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  // Because we're using the internal padding to keep track of the changes we
  // make to the leading margin to handle Fitts' Law, it's easier to just
  // allocate the property once and modify the value.
  SetProperty(views::kInternalPaddingKey, gfx::Insets());

  UpdateHighlightBackgroundAndInsets();
}

ToolbarButton::~ToolbarButton() {}

void ToolbarButton::Init() {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

void ToolbarButton::SetHighlightColor(base::Optional<SkColor> color) {
  if (highlight_color_ == color)
    return;

  highlight_color_ = color;
  UpdateHighlightBackgroundAndInsets();
}

void ToolbarButton::SetText(const base::string16& text) {
  LabelButton::SetText(text);
  UpdateHighlightBackgroundAndInsets();
}

void ToolbarButton::UpdateHighlightBackgroundAndInsets() {
  const int highlight_radius =
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MAXIMUM, size());
  if (!highlight_color_) {
    SetBackground(nullptr);
  } else {
    // ToolbarButtons are always the height the location bar.
    const gfx::Insets bg_insets(
        (height() - GetLayoutConstant(LOCATION_BAR_HEIGHT)) / 2);
    const SkColor bg_color =
        SkColorSetA(*highlight_color_, kToolbarButtonBackgroundAlpha);
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(bg_color, highlight_radius,
                                                    bg_insets)));
    SetEnabledTextColors(*highlight_color_);
  }

  gfx::Insets new_insets = GetLayoutInsets(TOOLBAR_BUTTON) +
                           layout_inset_delta_ +
                           *GetProperty(views::kInternalPaddingKey);

  if (!GetText().empty()) {
    const int text_side_inset = highlight_radius / 2;

    // Some subclasses (AvatarToolbarButton) may be change alignment. This adds
    // an inset to the text-label side.
    if (GetHorizontalAlignment() == gfx::ALIGN_RIGHT) {
      new_insets += gfx::Insets(0, text_side_inset, 0, 0);
    } else {
      new_insets += gfx::Insets(0, 0, 0, text_side_inset);
    }
  }

  if (!border() || new_insets != border()->GetInsets())
    SetBorder(views::CreateEmptyBorder(new_insets));
}

void ToolbarButton::SetLayoutInsetDelta(const gfx::Insets& inset_delta) {
  if (layout_inset_delta_ == inset_delta)
    return;
  layout_inset_delta_ = inset_delta;
  UpdateHighlightBackgroundAndInsets();
}

void ToolbarButton::SetLeadingMargin(int margin) {
  gfx::Insets* const internal_padding = GetProperty(views::kInternalPaddingKey);
  if (internal_padding->left() == margin)
    return;
  internal_padding->set_left(margin);
  UpdateHighlightBackgroundAndInsets();
}

void ToolbarButton::ClearPendingMenu() {
  show_menu_factory_.InvalidateWeakPtrs();
}

bool ToolbarButton::IsMenuShowing() const {
  return menu_showing_;
}

void ToolbarButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetToolbarButtonHighlightPath(this, *GetProperty(views::kInternalPaddingKey));

  UpdateHighlightBackgroundAndInsets();
  LabelButton::OnBoundsChanged(previous_bounds);
}

void ToolbarButton::OnThemeChanged() {
  LabelButton::OnThemeChanged();
  if (installable_ink_drop_)
    installable_ink_drop_->SetConfig(GetToolbarInstallableInkDropConfig(this));
}

gfx::Rect ToolbarButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  gfx::Insets insets =
      GetToolbarInkDropInsets(this, *GetProperty(views::kInternalPaddingKey));
  // If the button is extended, don't inset the leading edge. The anchored menu
  // should extend to the screen edge as well so the menu is easier to hit
  // (Fitts's law).
  // TODO(pbos): Make sure the button is aware of that it is being extended or
  // not (leading_margin_ cannot be used as it can be 0 in fullscreen on Touch).
  // When this is implemented, use 0 as a replacement for leading_margin_ in
  // fullscreen only. Always keep the rest.
  insets.Set(insets.top(), 0, insets.bottom(), 0);
  bounds.Inset(insets);
  return bounds;
}

bool ToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  if (trigger_menu_on_long_press_ && IsTriggerableEvent(event) &&
      GetEnabled() && ShouldShowMenu() && HitTestPoint(event.location())) {
    // Store the y pos of the mouse coordinates so we can use them later to
    // determine if the user dragged the mouse down (which should pop up the
    // drag down menu immediately, instead of waiting for the timer)
    y_position_on_lbuttondown_ = event.y();

    // Schedule a task that will show the menu.
    const int kMenuTimerDelay = 500;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ToolbarButton::ShowDropDownMenu,
                       show_menu_factory_.GetWeakPtr(),
                       ui::GetMenuSourceTypeForEvent(event)),
        base::TimeDelta::FromMilliseconds(kMenuTimerDelay));
  }

  return LabelButton::OnMousePressed(event);
}

bool ToolbarButton::OnMouseDragged(const ui::MouseEvent& event) {
  bool result = LabelButton::OnMouseDragged(event);

  if (trigger_menu_on_long_press_ && show_menu_factory_.HasWeakPtrs()) {
    // If the mouse is dragged to a y position lower than where it was when
    // clicked then we should not wait for the menu to appear but show
    // it immediately.
    if (event.y() > y_position_on_lbuttondown_ + GetHorizontalDragThreshold()) {
      show_menu_factory_.InvalidateWeakPtrs();
      ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
    }
  }

  return result;
}

void ToolbarButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event) ||
      (event.IsRightMouseButton() && !HitTestPoint(event.location()))) {
    LabelButton::OnMouseReleased(event);
  }

  if (IsTriggerableEvent(event))
    show_menu_factory_.InvalidateWeakPtrs();
}

void ToolbarButton::OnMouseCaptureLost() {}

void ToolbarButton::OnMouseExited(const ui::MouseEvent& event) {
  // Starting a drag results in a MouseExited, we need to ignore it.
  // A right click release triggers an exit event. We want to
  // remain in a PUSHED state until the drop down menu closes.
  if (state() != STATE_DISABLED && !InDrag() && state() != STATE_PRESSED)
    SetState(STATE_NORMAL);
}

void ToolbarButton::OnGestureEvent(ui::GestureEvent* event) {
  if (menu_showing_) {
    // While dropdown menu is showing the button should not handle gestures.
    event->StopPropagation();
    return;
  }

  LabelButton::OnGestureEvent(event);
}

void ToolbarButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kButton;
  node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
  if (GetEnabled())
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kPress);
}

std::unique_ptr<views::InkDrop> ToolbarButton::CreateInkDrop() {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  return views::LabelButton::CreateInkDrop();
}

std::unique_ptr<views::InkDropHighlight> ToolbarButton::CreateInkDropHighlight()
    const {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  return CreateToolbarInkDropHighlight(this);
}

SkColor ToolbarButton::GetInkDropBaseColor() const {
  // Ensure this doesn't get called when InstallableInkDrops are enabled.
  DCHECK(!base::FeatureList::IsEnabled(views::kInstallableInkDropFeature));
  if (highlight_color_)
    return *highlight_color_;
  return GetToolbarInkDropBaseColor(this);
}

views::InkDrop* ToolbarButton::GetInkDrop() {
  if (installable_ink_drop_)
    return installable_ink_drop_.get();
  return views::LabelButton::GetInkDrop();
}

void ToolbarButton::ShowContextMenuForViewImpl(View* source,
                                               const gfx::Point& point,
                                               ui::MenuSourceType source_type) {
  if (!GetEnabled())
    return;

  show_menu_factory_.InvalidateWeakPtrs();
  ShowDropDownMenu(source_type);
}

// static
SkColor ToolbarButton::AdjustHighlightColorForContrast(
    const ui::ThemeProvider* theme_provider,
    SkColor desired_dark_color,
    SkColor desired_light_color,
    SkColor dark_extreme,
    SkColor light_extreme) {
  if (!theme_provider)
    return desired_light_color;
  const SkColor toolbar_color =
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR);
  const SkColor contrasting_color = color_utils::PickContrastingColor(
      desired_dark_color, desired_light_color, toolbar_color);
  const SkColor limit =
      contrasting_color == desired_dark_color ? dark_extreme : light_extreme;
  // Setting highlight color will set the text to the highlight color, and the
  // background to the same color with a low alpha. This means that our target
  // contrast is between the text (the highlight color) and a blend of the
  // highlight color and the toolbar color.
  const SkColor base_color = color_utils::AlphaBlend(
      contrasting_color, toolbar_color, kToolbarButtonBackgroundAlpha);

  // Add a fudge factor to the minimum contrast ratio since we'll actually be
  // blending with the adjusted color.
  return color_utils::BlendForMinContrast(
             contrasting_color, base_color, limit,
             color_utils::kMinimumReadableContrastRatio * 1.05)
      .color;
}

bool ToolbarButton::ShouldShowMenu() {
  return model_ != nullptr;
}

void ToolbarButton::ShowDropDownMenu(ui::MenuSourceType source_type) {
  if (!ShouldShowMenu())
    return;

  gfx::Rect menu_anchor_bounds = GetAnchorBoundsInScreen();

#if defined(OS_CHROMEOS)
  // A window won't overlap between displays on ChromeOS.
  // Use the left bound of the display on which
  // the menu button exists.
  gfx::NativeView view = GetWidget()->GetNativeView();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);
  int left_bound = display.bounds().x();
#else
  // The window might be positioned over the edge between two screens. We'll
  // want to position the dropdown on the screen the mouse cursor is on.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display =
      screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  int left_bound = display.bounds().x();
#endif
  if (menu_anchor_bounds.x() < left_bound)
    menu_anchor_bounds.set_x(left_bound);

  // Make the button look depressed while the menu is open.
  SetState(STATE_PRESSED);

  menu_showing_ = true;

  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr /* event */);

  // Exit if the model is null. Although ToolbarButton::ShouldShowMenu()
  // performs the same check, its overrides may not.
  if (!model_)
    return;

  if (tab_strip_model_ && !tab_strip_model_->GetActiveWebContents())
    return;

  // Create and run menu.
  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      model_.get(), base::BindRepeating(&ToolbarButton::OnMenuClosed,
                                        base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(triggerable_event_flags());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_adapter_->CreateMenu(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, menu_anchor_bounds,
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void ToolbarButton::OnMenuClosed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr /* event */);

  menu_showing_ = false;

  // Set the state back to normal after the drop down menu is closed.
  if (state() != STATE_DISABLED) {
    GetInkDrop()->SetHovered(IsMouseHovered());
    SetState(STATE_NORMAL);
  }

  menu_runner_.reset();
  menu_model_adapter_.reset();
}

const char* ToolbarButton::GetClassName() const {
  return "ToolbarButton";
}
