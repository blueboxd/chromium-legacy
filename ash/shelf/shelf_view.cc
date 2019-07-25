// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/screen_util.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_context_menu_model.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

using gfx::Animation;
using views::View;

namespace ash {

// The distance of the cursor from the outer rim of the shelf before it
// separates.
constexpr int kRipOffDistance = 48;

// The rip off drag and drop proxy image should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// The opacity represents that this partially disappeared item will get removed.
constexpr float kDraggedImageOpacity = 0.5f;

namespace {

// White with ~20% opacity.
constexpr SkColor kSeparatorColor = SkColorSetARGB(0x32, 0xFF, 0xFF, 0xFF);

// The dimensions, in pixels, of the separator between pinned and unpinned
// items.
constexpr int kSeparatorSize = 20;
constexpr int kSeparatorThickness = 1;

// The margin on either side of the group of app icons (including the overflow
// button).
constexpr int kAppIconGroupMargin = 16;

// Inset from the bubble bounds to the bounds beyond which dragging triggers
// scrolling.
constexpr int kScrollTriggerBoundsInsetDips = 28;

// Time delay after which the scrolling speed will be increased.
constexpr base::TimeDelta kDragScrollSpeedIncreaseDelay =
    base::TimeDelta::FromSeconds(1);

// Time interval at which to scroll the overflow bubble for dragging.
constexpr base::TimeDelta kDragScrollInterval =
    base::TimeDelta::FromMilliseconds(17);

// How far to scroll the overflow bubble each time.
constexpr int kDragSlowScrollDeltaDips = 3;
constexpr int kDragFastScrollDeltaDips = 6;

// Helper to check if tablet mode is enabled.
bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Returns the size occupied by |count| app icons. If |with_overflow| is
// true, returns the size of |count| app icons followed by an overflow
// button.
int GetSizeOfAppIcons(int count, bool with_overflow) {
  const int control_size = ShelfConstants::control_size();
  const int button_spacing = ShelfConstants::button_spacing();
  const int overflow_button_margin = ShelfConstants::overflow_button_margin();

  if (count == 0)
    return with_overflow ? control_size + 2 * overflow_button_margin : 0;

  const int app_size = count * ShelfConstants::button_size();
  int overflow_size = 0;
  int total_padding = button_spacing * (count - 1);
  if (with_overflow) {
    overflow_size += control_size;
    total_padding += button_spacing + 2 * overflow_button_margin;
  }
  return app_size + total_padding + overflow_size;
}

// A class to temporarily disable a given bounds animator.
class BoundsAnimatorDisabler {
 public:
  explicit BoundsAnimatorDisabler(views::BoundsAnimator* bounds_animator)
      : old_duration_(bounds_animator->GetAnimationDuration()),
        bounds_animator_(bounds_animator) {
    bounds_animator_->SetAnimationDuration(1);
  }

  ~BoundsAnimatorDisabler() {
    bounds_animator_->SetAnimationDuration(old_duration_);
  }

 private:
  // The previous animation duration.
  int old_duration_;
  // The bounds animator which gets used.
  views::BoundsAnimator* bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorDisabler);
};

// Custom FocusSearch used to navigate the shelf in the order items are in
// the ViewModel.
class ShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ShelfFocusSearch(ShelfView* shelf_view)
      : FocusSearch(nullptr, true, true), shelf_view_(shelf_view) {}
  ~ShelfFocusSearch() override = default;

  // views::FocusSearch:
  View* FindNextFocusableView(
      View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      View** focus_traversable_view) override {
    // Build a list of all views that we are able to focus: 1) items from the
    // main shelf, 2) overflow button, 3) items from overflow if applicable.
    std::vector<views::View*> focusable_views;
    ShelfView* main_shelf = shelf_view_->main_shelf();
    ShelfView* overflow_shelf = shelf_view_->overflow_shelf();

    if (IsTabletModeEnabled())
      focusable_views.push_back(main_shelf->GetBackButton());

    focusable_views.push_back(main_shelf->GetHomeButton());
    for (int i = main_shelf->first_visible_index();
         i <= main_shelf->last_visible_index(); ++i) {
      focusable_views.push_back(main_shelf->view_model()->view_at(i));
    }
    if (main_shelf->GetOverflowButton()->GetVisible())
      focusable_views.push_back(main_shelf->GetOverflowButton());
    const int overflow_cutoff = static_cast<int>(focusable_views.size());
    if (main_shelf->IsShowingOverflowBubble() && overflow_shelf) {
      for (int i = overflow_shelf->first_visible_index();
           i <= overflow_shelf->last_visible_index(); ++i) {
        focusable_views.push_back(overflow_shelf->view_model()->view_at(i));
      }
    }

    // Where are we starting from?
    int start_index = 0;
    for (size_t i = 0; i < focusable_views.size(); ++i) {
      if (focusable_views[i] == starting_view) {
        start_index = i;
        break;
      }
    }
    int new_index =
        start_index +
        (search_direction == FocusSearch::SearchDirection::kBackwards ? -1 : 1);
    // Loop around.
    if (new_index < 0)
      new_index = focusable_views.size() - 1;
    else if (new_index >= static_cast<int>(focusable_views.size()))
      new_index = 0;

    if (new_index >= overflow_cutoff)
      shelf_view_->shelf_widget()->set_activated_from_overflow_bubble(true);
    return focusable_views[new_index];
  }

 private:
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(ShelfFocusSearch);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit FadeInAnimationDelegate(views::View* view) : view_(view) {}
  ~FadeInAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }
  void AnimationCanceled(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(FadeInAnimationDelegate);
};

// Returns the id of the display on which |view| is shown.
int64_t GetDisplayIdForView(const View* view) {
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

// Whether |item_view| is a ShelfAppButton and its state is STATE_DRAGGING.
bool ShelfButtonIsInDrag(const ShelfItemType item_type,
                         const views::View* item_view) {
  switch (item_type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
      return static_cast<const ShelfAppButton*>(item_view)->state() &
             ShelfAppButton::STATE_DRAGGING;
    case TYPE_DIALOG:
    case TYPE_UNDEFINED:
      return false;
  }
}

}  // namespace

// AnimationDelegate used when deleting an item. This steadily decreased the
// opacity of the layer as the animation progress.
class ShelfView::FadeOutAnimationDelegate : public gfx::AnimationDelegate {
 public:
  FadeOutAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host), view_(view) {}
  ~FadeOutAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationProgressed(const Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  void AnimationEnded(const Animation* animation) override {
    shelf_view_->OnFadeOutAnimationEnded();
  }
  void AnimationCanceled(const Animation* animation) override {}

 private:
  ShelfView* shelf_view_;
  std::unique_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(FadeOutAnimationDelegate);
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class ShelfView::StartFadeAnimationDelegate : public gfx::AnimationDelegate {
 public:
  StartFadeAnimationDelegate(ShelfView* host, views::View* view)
      : shelf_view_(host), view_(view) {}
  ~StartFadeAnimationDelegate() override = default;

  // AnimationDelegate overrides:
  void AnimationEnded(const Animation* animation) override {
    shelf_view_->FadeIn(view_);
  }
  void AnimationCanceled(const Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
  }

 private:
  ShelfView* shelf_view_;
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(StartFadeAnimationDelegate);
};

// static
const int ShelfView::kMinimumDragDistance = 8;

ShelfView::ShelfView(ShelfModel* model, Shelf* shelf)
    : model_(model),
      shelf_(shelf),
      view_model_(std::make_unique<views::ViewModel>()),
      bounds_animator_(std::make_unique<views::BoundsAnimator>(this)),
      tooltip_(this),
      focus_search_(std::make_unique<ShelfFocusSearch>(this)),
      weak_factory_(this) {
  DCHECK(model_);
  DCHECK(shelf_);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  bounds_animator_->AddObserver(this);
  set_context_menu_controller(this);

  announcement_view_ = new views::View();
  AddChildView(announcement_view_);
}

ShelfView::~ShelfView() {
  // Shell destroys the TabletModeController before destroying all root windows.
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  bounds_animator_->RemoveObserver(this);
  model_->RemoveObserver(this);
}

void ShelfView::Init() {
  // Add the background view behind the app list and back buttons first, so
  // that other views will appear above it.
  back_and_app_list_background_ = new views::View();
  back_and_app_list_background_->set_can_process_events_within_subtree(false);
  back_and_app_list_background_->SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kShelfControlPermanentHighlightBackground,
          ShelfConstants::control_border_radius())));
  ConfigureChildView(back_and_app_list_background_);
  AddChildView(back_and_app_list_background_);

  separator_ = new views::Separator();
  separator_->SetColor(kSeparatorColor);
  separator_->SetPreferredHeight(kSeparatorSize);
  separator_->SetVisible(false);
  ConfigureChildView(separator_);
  AddChildView(separator_);

  model()->AddObserver(this);

  const ShelfItems& items(model_->items());

  std::unique_ptr<BackButton> back_button_ptr =
      std::make_unique<BackButton>(shelf_);
  ConfigureChildView(back_button_ptr.get());
  back_button_ = AddChildView(std::move(back_button_ptr));

  std::unique_ptr<HomeButton> home_button_ptr =
      std::make_unique<HomeButton>(shelf_);
  ConfigureChildView(home_button_ptr.get());
  home_button_ = AddChildView(std::move(home_button_ptr));
  home_button_->set_context_menu_controller(this);

  for (ShelfItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    // TODO(manucornet): Remove this if statement once home and back are not
    // allowed shelf item types in the model any longer.
    if (!child)
      continue;
    child->SetPaintToLayer();
    view_model_->Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }
  overflow_button_ = new OverflowButton(this);
  ConfigureChildView(overflow_button_);
  AddChildView(overflow_button_);
  UpdateBackButton();

  // We'll layout when our bounds change.
}

gfx::Rect ShelfView::GetIdealBoundsOfItemIcon(const ShelfID& id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0 || last_visible_index_ < 0 || index >= view_model_->view_size())
    return gfx::Rect();

  // Map items in the overflow bubble to the overflow button.
  if (index > last_visible_index_)
    return GetMirroredRect(overflow_button_->bounds());

  const gfx::Rect& ideal_bounds(view_model_->ideal_bounds(index));
  views::View* view = view_model_->view_at(index);

  CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
  gfx::Rect icon_bounds = button->GetIconBounds();
  return gfx::Rect(GetMirroredXWithWidthInView(
                       ideal_bounds.x() + icon_bounds.x(), icon_bounds.width()),
                   ideal_bounds.y() + icon_bounds.y(), icon_bounds.width(),
                   icon_bounds.height());
}

bool ShelfView::IsShowingMenu() const {
  return shelf_menu_model_adapter_ &&
         shelf_menu_model_adapter_->IsShowingMenu();
}

bool ShelfView::IsShowingOverflowBubble() const {
  return overflow_bubble_.get() && overflow_bubble_->IsShowing();
}

HomeButton* ShelfView::GetHomeButton() const {
  return home_button_;
}

BackButton* ShelfView::GetBackButton() const {
  return back_button_;
}

OverflowButton* ShelfView::GetOverflowButton() const {
  return overflow_button_;
}

void ShelfView::UpdateVisibleShelfItemBoundsUnion() {
  visible_shelf_item_bounds_union_.SetRect(0, 0, 0, 0);
  for (int i = std::max(0, first_visible_index_); i <= last_visible_index_;
       ++i) {
    const views::View* child = view_model_->view_at(i);
    if (ShouldShowTooltipForView(child))
      visible_shelf_item_bounds_union_.Union(child->GetMirroredBounds());
  }
  // Also include the overflow button if it is visible.
  if (overflow_button_->GetVisible()) {
    visible_shelf_item_bounds_union_.Union(
        overflow_button_->GetMirroredBounds());
  }
}

bool ShelfView::ShouldHideTooltip(const gfx::Point& cursor_location) const {
  // If this is the home button, only show the tooltip if the app list is
  // not already showing.
  const HomeButton* home_button = GetHomeButton();
  if (home_button &&
      home_button->GetMirroredBounds().Contains(cursor_location)) {
    return home_button->IsShowingAppList();
  }
  return !visible_shelf_item_bounds_union_.Contains(cursor_location);
}

bool ShelfView::ShouldShowTooltipForView(const views::View* view) const {
  // If this is the home button, only show the tooltip if the app list is
  // not already showing.
  if (view == GetHomeButton())
    return !GetHomeButton()->IsShowingAppList();
  if (view == overflow_button_)
    return true;
  // Don't show a tooltip for a view that's currently being dragged.
  if (view == drag_view_)
    return false;

  return ShelfItemForView(view) && !IsShowingMenuForView(view);
}

base::string16 ShelfView::GetTitleForView(const views::View* view) const {
  if (view == back_button_)
    return back_button_->GetAccessibleName();
  if (view == home_button_)
    return home_button_->GetAccessibleName();
  if (view == overflow_button_)
    return overflow_button_->GetAccessibleName();

  const ShelfItem* item = ShelfItemForView(view);
  return item ? item->title : base::string16();
}

void ShelfView::ToggleOverflowBubble() {
  if (IsShowingOverflowBubble()) {
    overflow_bubble_->Hide();
    return;
  }

  if (!overflow_bubble_)
    overflow_bubble_.reset(new OverflowBubble(shelf_));

  ShelfView* overflow_view = new ShelfView(model_, shelf_);
  overflow_view->overflow_mode_ = true;
  overflow_view->Init();
  overflow_view->set_owner_overflow_bubble(overflow_bubble_.get());
  overflow_view->OnShelfAlignmentChanged(
      GetWidget()->GetNativeWindow()->GetRootWindow());
  overflow_view->main_shelf_ = this;
  UpdateOverflowRange(overflow_view);

  overflow_bubble_->Show(overflow_button_, overflow_view);

  shelf_->UpdateVisibilityState();
}

gfx::Rect ShelfView::GetVisibleItemsBoundsInScreen() {
  gfx::Size preferred_size = GetPreferredSize();
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, preferred_size);
}

gfx::Size ShelfView::CalculatePreferredSize() const {
  if (model_->item_count() == 0) {
    // There are no apps.
    return shelf_->IsHorizontalAlignment()
               ? gfx::Size(GetHomeButton()->bounds().right(),
                           ShelfConstants::shelf_size())
               : gfx::Size(ShelfConstants::shelf_size(),
                           GetHomeButton()->bounds().bottom());
  }

  int last_button_index = last_visible_index_;
  if (!is_overflow_mode() && overflow_button_ && overflow_button_->GetVisible())
    ++last_button_index;

  // When an item is dragged off from the overflow bubble, it is moved to last
  // position and and changed to invisible. Overflow bubble size should be
  // shrunk to fit only for visible items.
  // If |dragged_to_another_shelf_| is set, there will be no
  // invisible items in the shelf.
  if (is_overflow_mode() && dragged_off_shelf_ && !dragged_to_another_shelf_ &&
      RemovableByRipOff(view_model_->GetIndexOfView(drag_view_)) == REMOVABLE)
    last_button_index--;

  const gfx::Rect last_button_bounds =
      last_button_index >= first_visible_index_
          ? view_model_->ideal_bounds(last_button_index)
          : gfx::Rect(gfx::Size(ShelfConstants::shelf_size(),
                                ShelfConstants::shelf_size()));

  if (shelf_->IsHorizontalAlignment())
    return gfx::Size(last_button_bounds.right(), ShelfConstants::shelf_size());

  return gfx::Size(ShelfConstants::shelf_size(), last_button_bounds.bottom());
}

void ShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // This bounds change is produced by the shelf movement (rotation, alignment
  // change, etc.) and all content has to follow. Using an animation at that
  // time would produce a time lag since the animation of the BoundsAnimator has
  // itself a delay before it arrives at the required location. As such we tell
  // the animator to go there immediately. We still want to use an animation
  // when the bounds change is caused by entering or exiting tablet mode.
  if (shelf_->is_tablet_mode_animation_running()) {
    AnimateToIdealBounds();
    if (IsShowingOverflowBubble()) {
      overflow_bubble_->bubble_view()->shelf_view()->OnBoundsChanged(
          previous_bounds);
    }
    return;
  }

  BoundsAnimatorDisabler disabler(bounds_animator_.get());

  LayoutToIdealBounds();
  shelf_->NotifyShelfIconPositionsChanged();

  if (IsShowingOverflowBubble())
    overflow_bubble_->Hide();
}

bool ShelfView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.IsControlDown() &&
      ash::keyboard_util::IsArrowKeyCode(event.key_code())) {
    bool swap_with_next = (event.key_code() == ui::VKEY_DOWN ||
                           event.key_code() == ui::VKEY_RIGHT);
    SwapButtons(GetFocusManager()->GetFocusedView(), swap_with_next);
    return true;
  }
  return views::View::OnKeyPressed(event);
}

views::FocusTraversable* ShelfView::GetPaneFocusTraversable() {
  return this;
}

const char* ShelfView::GetClassName() const {
  return "ShelfView";
}

void ShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

View* ShelfView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // Similar implementation as views::View, but without going into each
  // child's subviews.
  View::Views children = GetChildrenInZOrder();
  for (auto* child : base::Reversed(children)) {
    if (!child->GetVisible())
      continue;

    gfx::Point point_in_child_coords(point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->HitTestPoint(point_in_child_coords) &&
        ShouldShowTooltipForView(child)) {
      return child;
    }
  }
  // If none of our children qualifies, just return the shelf view itself.
  return this;
}

void ShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  if (is_overflow_mode()) {
    main_shelf()->OnShelfButtonAboutToRequestFocusFromTabTraversal(button,
                                                                   reverse);
    return;
  }

  if (ShouldFocusOut(reverse, button))
    shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kShelfView);
}

void ShelfView::ButtonPressed(views::Button* sender,
                              const ui::Event& event,
                              views::InkDrop* ink_drop) {

  if (sender == overflow_button_) {
    ToggleOverflowBubble();
    shelf_button_pressed_metric_tracker_.ButtonPressed(event, sender,
                                                       SHELF_ACTION_NONE);
    return;
  }

  // None of the checks in ShouldEventActivateButton() affects overflow button.
  // So, it is safe to be checked after handling overflow button.
  if (!ShouldEventActivateButton(sender, event)) {
    ink_drop->SnapToHidden();
    return;
  }

  // Prevent concurrent requests that may show application or context menus.
  if (!item_awaiting_response_.IsNull()) {
    const ShelfItem* item = ShelfItemForView(sender);
    if (item && item->id != item_awaiting_response_)
      ink_drop->AnimateToState(views::InkDropState::DEACTIVATED);
    return;
  }

  // Ensure the keyboard is hidden and stays hidden (as long as it isn't locked)
  if (keyboard::KeyboardUIController::Get()->IsEnabled())
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

  // Close the overflow bubble if an item on either shelf is clicked. Press
  // events elsewhere will close the overflow shelf via OverflowBubble's
  // EventHandler functionality.
  ShelfView* shelf_view = main_shelf_ ? main_shelf_ : this;
  if (shelf_view->IsShowingOverflowBubble())
    shelf_view->ToggleOverflowBubble();

  // Record the index for the last pressed shelf item.
  last_pressed_index_ = view_model_->GetIndexOfView(sender);
  DCHECK_LT(-1, last_pressed_index_);

  // Place new windows on the same display as the button.
  aura::Window* window = sender->GetWidget()->GetNativeWindow();
  scoped_root_window_for_new_windows_ =
      std::make_unique<ScopedRootWindowForNewWindows>(window->GetRootWindow());

  // Slow down activation animations if Control key is pressed.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> slowing_animations;
  if (event.IsControlDown()) {
    slowing_animations.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  }

  // Collect usage statistics before we decide what to do with the click.
  switch (model_->items()[last_pressed_index_].type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_CLICK_ON_APP);
      break;

    case TYPE_DIALOG:
      break;

    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
      break;
  }

  // Record the current AppListViewState to be used later for metrics. The
  // AppListViewState will change on app launch, so this will record the
  // AppListViewState before the app was launched.
  if (Shell::Get()->app_list_controller()) {
    recorded_app_list_view_state_ =
        Shell::Get()->app_list_controller()->GetAppListViewState();
    recorded_home_launcher_shown_ =
        Shell::Get()->app_list_controller()->presenter()->home_launcher_shown();
  }

  // Run AfterItemSelected directly if the item has no delegate (ie. in tests).
  const ShelfItem& item = model_->items()[last_pressed_index_];
  if (!model_->GetShelfItemDelegate(item.id)) {
    AfterItemSelected(item, sender, ui::Event::Clone(event), ink_drop,
                      SHELF_ACTION_NONE, {});
    return;
  }

  // Notify the item of its selection; handle the result in AfterItemSelected.
  item_awaiting_response_ = item.id;
  model_->GetShelfItemDelegate(item.id)->ItemSelected(
      ui::Event::Clone(event), GetDisplayIdForView(this), LAUNCH_FROM_SHELF,
      base::BindOnce(&ShelfView::AfterItemSelected, weak_factory_.GetWeakPtr(),
                     item, sender, base::Passed(ui::Event::Clone(event)),
                     ink_drop));
}

bool ShelfView::IsShowingMenuForView(const views::View* view) const {
  return IsShowingMenu() &&
         shelf_menu_model_adapter_->IsShowingMenuForView(*view);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView, FocusTraversable implementation:

views::FocusSearch* ShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

View* ShelfView::GetFocusTraversableParentView() {
  return this;
}

void ShelfView::CreateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    const gfx::Vector2d& cursor_offset_from_center,
    float scale_factor) {
  drag_replaced_view_ = replaced_view;
  aura::Window* root_window =
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  drag_image_ = std::make_unique<DragImageView>(
      root_window, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  drag_image_offset_ = gfx::Vector2d(size.width() / 2, size.height() / 2) +
                       cursor_offset_from_center;
  gfx::Rect drag_image_bounds(
      location_in_screen_coordinates - drag_image_offset_, size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);
  drag_image_->SetWidgetVisible(true);
}

void ShelfView::OnTabletModeStarted() {
  // Close all menus when tablet mode starts to ensure that the clamshell only
  // context menu options are not available in tablet mode.
  if (shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::OnTabletModeEnded() {
  // Close all menus when tablet mode ends so that menu options are kept
  // consistent with device state.
  if (shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::OnVirtualKeyboardVisibilityChanged() {
  LayoutToIdealBounds();
}

bool ShelfView::ShouldEventActivateButton(View* view, const ui::Event& event) {
  // This only applies to app buttons.
  DCHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  if (dragging())
    return false;

  // Ignore if we are already in a pointer event sequence started with a repost
  // event on the same shelf item. See crbug.com/343005 for more detail.
  if (is_repost_event_on_same_item_)
    return false;

  // Don't activate the item twice on double-click. Otherwise the window starts
  // animating open due to the first click, then immediately minimizes due to
  // the second click. The user most likely intended to open or minimize the
  // item once, not do both.
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK)
    return false;

  const bool repost = IsRepostEvent(event);

  // Ignore if this is a repost event on the last pressed shelf item.
  int index = view_model_->GetIndexOfView(view);
  if (index == -1)
    return false;
  return !repost || last_pressed_index_ != index;
}

void ShelfView::CreateDragIconProxyByLocationWithNoAnimation(
    const gfx::Point& origin_in_screen_coordinates,
    const gfx::ImageSkia& icon,
    views::View* replaced_view,
    float scale_factor,
    int blur_radius) {
  drag_replaced_view_ = replaced_view;
  aura::Window* root_window =
      drag_replaced_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  drag_image_ = std::make_unique<DragImageView>(
      root_window, ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  if (blur_radius > 0)
    SetDragImageBlur(icon.size(), blur_radius);
  drag_image_->SetImage(icon);
  gfx::Size size = drag_image_->GetPreferredSize();
  size.set_width(size.width() * scale_factor);
  size.set_height(size.height() * scale_factor);
  gfx::Rect drag_image_bounds(origin_in_screen_coordinates, size);
  drag_image_->SetBoundsInScreen(drag_image_bounds);

  // Turn off the default visibility animation.
  drag_image_->GetWidget()->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  drag_image_->SetWidgetVisible(true);
}

void ShelfView::UpdateDragIconProxy(
    const gfx::Point& location_in_screen_coordinates) {
  // TODO(jennyz): Investigate why drag_image_ becomes null at this point per
  // crbug.com/34722, while the app list item is still being dragged around.
  if (drag_image_) {
    drag_image_->SetScreenPosition(location_in_screen_coordinates -
                                   drag_image_offset_);
  }
}

void ShelfView::UpdateDragIconProxyByLocation(
    const gfx::Point& origin_in_screen_coordinates) {
  if (drag_image_)
    drag_image_->SetScreenPosition(origin_in_screen_coordinates);
}

bool ShelfView::IsDraggedView(const views::View* view) const {
  return drag_view_ == view;
}

const std::vector<aura::Window*> ShelfView::GetOpenWindowsForShelfView(
    views::View* view) {
  std::vector<aura::Window*> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  std::vector<aura::Window*> open_windows;
  const ShelfItem* item = ShelfItemForView(view);

  // The concept of a list of open windows doesn't make sense for something
  // that isn't an app shortcut: return an empty list.
  if (!item)
    return open_windows;

  for (auto* window : window_list) {
    const std::string window_app_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;
    if (window_app_id == item->id.app_id) {
      // TODO: In the very first version we only show one window. Add the proper
      // UI to show all windows for a given open app.
      open_windows.push_back(window);
    }
  }
  return open_windows;
}

views::View* ShelfView::FindFirstFocusableChild() {
  if (is_overflow_mode())
    return main_shelf()->FindFirstFocusableChild();
  return IsTabletModeEnabled() ? static_cast<views::View*>(GetBackButton())
                               : static_cast<views::View*>(GetHomeButton());
}

views::View* ShelfView::FindLastFocusableChild() {
  if (IsShowingOverflowBubble())
    return overflow_shelf()->FindLastFocusableChild();
  if (view_model_->view_size() == 0)
    return static_cast<views::View*>(GetHomeButton());
  return overflow_button_->GetVisible()
             ? overflow_button_
             : view_model_->view_at(last_visible_index());
}

views::View* ShelfView::FindFirstOrLastFocusableChild(bool last) {
  return last ? FindLastFocusableChild() : FindFirstFocusableChild();
}

// static
void ShelfView::ConfigureChildView(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void ShelfView::CalculateIdealBounds() {
  DCHECK(model()->item_count() == view_model()->view_size());

  const int button_spacing = ShelfConstants::button_spacing();
  const int available_size = shelf()->PrimaryAxisValue(width(), height());

  const int separator_index = GetSeparatorIndex();
  const AppCenteringStrategy app_centering_strategy =
      CalculateAppCenteringStrategy();

  // At this point we know that |last_visible_index_| is up to date.
  const bool virtual_keyboard_visible =
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible();
  // Don't show the separator if it isn't needed, or would appear after all
  // visible items.
  separator_->SetVisible(separator_index != -1 &&
                         separator_index < last_visible_index() &&
                         !virtual_keyboard_visible);

  int x = 0;
  int y = 0;
  if (!is_overflow_mode()) {
    // Calculate the bounds for the back and home buttons separately.
    CalculateBackAndHomeButtonsIdealBounds();

    int home_button_position =
        shelf()->PrimaryAxisValue(GetHomeButton()->ideal_bounds().right(),
                                  GetHomeButton()->ideal_bounds().bottom());

    x = shelf()->PrimaryAxisValue(home_button_position, 0);
    y = shelf()->PrimaryAxisValue(0, home_button_position);

    // Add the minimum padding required after the home button.
    x += shelf()->PrimaryAxisValue(kAppIconGroupMargin, 0);
    y += shelf()->PrimaryAxisValue(0, kAppIconGroupMargin);

    // Now add the necessary padding to center app icons.
    StatusAreaWidget* status_widget = shelf_widget()->status_area_widget();
    const int status_widget_size =
        status_widget ? shelf()->PrimaryAxisValue(
                            status_widget->GetWindowBoundsInScreen().width(),
                            status_widget->GetWindowBoundsInScreen().height())
                      : 0;
    const int screen_size = available_size + status_widget_size;

    const int available_size_for_app_icons = GetAvailableSpaceForAppIcons();
    const int icons_size = GetSizeOfAppIcons(number_of_visible_apps(),
                                             app_centering_strategy.overflow);
    int padding_for_centering = 0;

    if (app_centering_strategy.center_on_screen) {
      padding_for_centering = (screen_size - icons_size) / 2;
    } else {
      padding_for_centering =
          ShelfConstants::home_button_edge_spacing() +
          (IsTabletModeEnabled() ? 2 : 1) * ShelfConstants::control_size() +
          (IsTabletModeEnabled() ? button_spacing : 0) + kAppIconGroupMargin +
          (available_size_for_app_icons - icons_size) / 2;
    }

    if (padding_for_centering > home_button_position + kAppIconGroupMargin) {
      // Only shift buttons to the right, never let them interfere with the
      // left-aligned system buttons.
      x = shelf()->PrimaryAxisValue(padding_for_centering, 0);
      y = shelf()->PrimaryAxisValue(0, padding_for_centering);
    }
  }

  for (int i = 0; i < view_model()->view_size(); ++i) {
    if (is_overflow_mode() && i < first_visible_index()) {
      view_model()->set_ideal_bounds(i, gfx::Rect(x, y, 0, 0));
      continue;
    }

    const int button_size = ShelfConstants::button_size();

    view_model()->set_ideal_bounds(i,
                                   gfx::Rect(x, y, button_size, button_size));

    x = shelf()->PrimaryAxisValue(x + button_size + button_spacing, x);
    y = shelf()->PrimaryAxisValue(y, y + button_size + button_spacing);

    if (i == separator_index) {
      // Place the separator halfway between the two icons it separates,
      // vertically centered.
      int half_space = button_spacing / 2;
      int secondary_offset =
          (ShelfConstants::shelf_size() - kSeparatorSize) / 2;
      x -= shelf()->PrimaryAxisValue(half_space, 0);
      y -= shelf()->PrimaryAxisValue(0, half_space);
      separator_->SetBounds(
          x + shelf()->PrimaryAxisValue(0, secondary_offset),
          y + shelf()->PrimaryAxisValue(secondary_offset, 0),
          shelf()->PrimaryAxisValue(kSeparatorThickness, kSeparatorSize),
          shelf()->PrimaryAxisValue(kSeparatorSize, kSeparatorThickness));
      x += shelf()->PrimaryAxisValue(half_space, 0);
      y += shelf()->PrimaryAxisValue(0, half_space);
    }
  }

  if (is_overflow_mode()) {
    UpdateAllButtonsVisibilityInOverflowMode();
    return;
  }
  // In the main shelf, the first visible index is either the first app, or -1
  // if there are no apps.
  first_visible_index_ = view_model()->view_size() == 0 ? -1 : 0;

  for (int i = 0; i < view_model()->view_size(); ++i) {
    // To receive drag event continuously from |drag_view_| during the dragging
    // off from the shelf, don't make |drag_view_| invisible. It will be
    // eventually invisible and removed from the |view_model_| by
    // FinalizeRipOffDrag().
    if (dragged_off_shelf_ && view_model()->view_at(i) == drag_view())
      continue;
    // If the virtual keyboard is visible, do not show any apps.
    view_model()->view_at(i)->SetVisible(i <= last_visible_index() &&
                                         !virtual_keyboard_visible);
  }

  overflow_button_->SetVisible(app_centering_strategy.overflow);
  if (app_centering_strategy.overflow) {
    if (overflow_bubble() && overflow_bubble()->IsShowing())
      UpdateOverflowRange(overflow_bubble()->bubble_view()->shelf_view());
  } else {
    if (overflow_bubble())
      overflow_bubble()->Hide();
  }
}

views::View* ShelfView::CreateViewForItem(const ShelfItem& item) {
  views::View* view = nullptr;
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      ShelfAppButton* button = new ShelfAppButton(this);
      button->SetImage(item.image);
      button->ReflectItemStatus(item);
      view = button;
      break;
    }

    case TYPE_UNDEFINED:
      return nullptr;
  }

  view->set_context_menu_controller(this);

  ConfigureChildView(view);
  return view;
}

void ShelfView::LayoutAppListAndBackButtonHighlight() {
  // Don't show anything if this is the overflow menu.
  if (is_overflow_mode()) {
    back_and_app_list_background_->SetVisible(false);
    return;
  }
  const int edge_spacing = ShelfConstants::home_button_edge_spacing();
  // "Secondary" as in "orthogonal to the shelf primary axis".
  const int control_secondary_padding =
      (ShelfConstants::shelf_size() - ShelfConstants::control_size()) / 2;
  const int back_and_app_list_background_size =
      ShelfConstants::control_size() +
      (IsTabletModeEnabled()
           ? ShelfConstants::control_size() + ShelfConstants::button_spacing()
           : 0);
  back_and_app_list_background_->SetBounds(
      shelf()->PrimaryAxisValue(edge_spacing, control_secondary_padding),
      shelf()->PrimaryAxisValue(control_secondary_padding, edge_spacing),
      shelf()->PrimaryAxisValue(back_and_app_list_background_size,
                                ShelfConstants::control_size()),
      shelf()->PrimaryAxisValue(ShelfConstants::control_size(),
                                back_and_app_list_background_size));
}

void ShelfView::UpdateOverflowRange(ShelfView* overflow_view) const {
  const int first_overflow_index = last_visible_index_ + 1;
  DCHECK_LE(first_overflow_index, model_->item_count() - 1);
  DCHECK_LT(model_->item_count() - 1, view_model_->view_size());

  overflow_view->first_visible_index_ = first_overflow_index;
  overflow_view->last_visible_index_ = model_->item_count() - 1;
}

int ShelfView::GetAvailableSpaceForAppIcons() const {
  // Subtract space already allocated to the home button, and the back
  // button if applicable.
  return shelf()->PrimaryAxisValue(width(), height()) -
         ShelfConstants::home_button_edge_spacing() -
         (IsTabletModeEnabled() ? 2 : 1) * ShelfConstants::control_size() -
         (IsTabletModeEnabled() ? ShelfConstants::button_spacing() : 0) -
         2 * kAppIconGroupMargin;
}

int ShelfView::GetSeparatorIndex() const {
  for (int i = 0; i < model()->item_count() - 1; ++i) {
    if (IsItemPinned(model()->items()[i]) &&
        model()->items()[i + 1].type == TYPE_APP) {
      return i;
    }
  }
  return -1;
}

void ShelfView::CalculateBackAndHomeButtonsIdealBounds() {
  if (is_overflow_mode())
    return;

  const int control_size = ShelfConstants::control_size();
  const int button_size = ShelfConstants::button_size();
  const int button_spacing = ShelfConstants::button_spacing();
  const int edge_spacing = ShelfConstants::home_button_edge_spacing();

  int x = shelf()->PrimaryAxisValue(edge_spacing, 0);
  int y = shelf()->PrimaryAxisValue(0, edge_spacing);

  GetBackButton()->set_ideal_bounds(
      gfx::Rect(x, y, shelf()->PrimaryAxisValue(control_size, button_size),
                shelf()->PrimaryAxisValue(button_size, control_size)));

  // If we are not in tablet mode, the home button will get placed on top of
  // the (invisible) back button.
  if (IsTabletModeEnabled()) {
    x = shelf()->PrimaryAxisValue(x + control_size + button_spacing, x);
    y = shelf()->PrimaryAxisValue(y, y + control_size + button_spacing);
  }

  GetHomeButton()->set_ideal_bounds(
      gfx::Rect(x, y, shelf()->PrimaryAxisValue(control_size, button_size),
                shelf()->PrimaryAxisValue(button_size, control_size)));
}

ShelfView::AppCenteringStrategy ShelfView::CalculateAppCenteringStrategy() {
  // There are two possibilities. Either all the apps fit when centered
  // on the whole screen width, in which case we do that. Or, when space
  // becomes a little tight (which happens especially when the status area
  // is wider because of extra panels), we center apps on the available space.

  AppCenteringStrategy strategy;
  // This is only relevant for the main shelf.
  if (is_overflow_mode())
    return strategy;

  const int total_available_size = shelf()->PrimaryAxisValue(width(), height());
  StatusAreaWidget* status_widget = shelf_widget()->status_area_widget();
  const int status_widget_size =
      status_widget ? shelf()->PrimaryAxisValue(
                          status_widget->GetWindowBoundsInScreen().width(),
                          status_widget->GetWindowBoundsInScreen().height())
                    : 0;
  const int screen_size = total_available_size + status_widget_size;

  // An easy way to check whether the apps fit at the exact center of the
  // screen is to imagine that we have another status widget on the other
  // side (the status widget is always bigger than the home button plus
  // the back button if applicable) and see if the apps can fit in the middle.
  int available_space_for_screen_centering =
      screen_size - 2 * (status_widget_size + kAppIconGroupMargin);

  if (GetSizeOfAppIcons(view_model()->view_size(), false) <
      available_space_for_screen_centering) {
    // Everything fits in the center of the screen.
    last_visible_index_ = view_model()->view_size() - 1;
    strategy.center_on_screen = true;
    return strategy;
  }

  const int available_size_for_app_icons = GetAvailableSpaceForAppIcons();
  last_visible_index_ = -1;
  // We know that replacing the last app that fits with the overflow button
  // will not change the outcome, so we ignore that case for now.
  while (last_visible_index() < view_model()->view_size() - 1) {
    if (GetSizeOfAppIcons(last_visible_index() + 2, false) <=
        available_size_for_app_icons) {
      last_visible_index_++;
    } else {
      strategy.overflow = true;
      // Make space for the overflow button by showing one fewer app icon.
      last_visible_index_--;
      break;
    }
  }
  return strategy;
}

void ShelfView::UpdateAllButtonsVisibilityInOverflowMode() {
  // The overflow button is not shown in overflow mode.
  overflow_button_->SetVisible(false);
  DCHECK_LT(last_visible_index(), view_model()->view_size());
  for (int i = 0; i < view_model()->view_size(); ++i) {
    bool visible = i >= first_visible_index() && i <= last_visible_index();
    // To track the dragging of |drag_view_| continuously, its visibility
    // should be always true regardless of its position.
    if (dragged_to_another_shelf_ && view_model()->view_at(i) == drag_view())
      view_model()->view_at(i)->SetVisible(true);
    else
      view_model()->view_at(i)->SetVisible(visible);
  }
}

void ShelfView::DestroyDragIconProxy() {
  drag_image_.reset();
  drag_image_offset_ = gfx::Vector2d(0, 0);
}

bool ShelfView::StartDrag(const std::string& app_id,
                          const gfx::Point& location_in_screen_coordinates) {
  // Bail if an operation is already going on - or the cursor is not inside.
  // This could happen if mouse / touch operations overlap.
  if (!drag_and_drop_shelf_id_.IsNull() || app_id.empty() ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  // If the AppsGridView (which was dispatching this event) was opened by our
  // button, ShelfView dragging operations are locked and we have to unlock.
  CancelDrag(-1);
  drag_and_drop_item_pinned_ = false;
  drag_and_drop_shelf_id_ = ShelfID(app_id);
  // Check if the application is pinned - if not, we have to pin it so
  // that we can re-arrange the shelf order accordingly. Note that items have
  // to be pinned to give them the same (order) possibilities as a shortcut.
  // When an item is dragged from overflow to shelf, IsShowingOverflowBubble()
  // returns true. At this time, we don't need to pin the item.
  if (!IsShowingOverflowBubble() && !model_->IsAppPinned(app_id)) {
    model_->PinAppWithID(app_id);
    drag_and_drop_item_pinned_ = true;
  }
  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  DCHECK(drag_and_drop_view);

  // Since there is already an icon presented by the caller, we hide this item
  // for now. That has to be done by reducing the size since the visibility will
  // change once a regrouping animation is performed.
  pre_drag_and_drop_size_ = drag_and_drop_view->size();
  drag_and_drop_view->SetSize(gfx::Size());

  // First we have to center the mouse cursor over the item.
  gfx::Point pt = drag_and_drop_view->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  wm::ConvertPointFromScreen(
      window_util::GetRootWindowAt(location_in_screen_coordinates),
      &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerPressedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);

  // Drag the item where it really belongs.
  Drag(location_in_screen_coordinates);
  return true;
}

bool ShelfView::Drag(const gfx::Point& location_in_screen_coordinates) {
  if (drag_and_drop_shelf_id_.IsNull() ||
      !GetBoundsInScreen().Contains(location_in_screen_coordinates))
    return false;

  gfx::Point pt = location_in_screen_coordinates;
  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  ConvertPointFromScreen(drag_and_drop_view, &pt);
  gfx::Point point_in_root = location_in_screen_coordinates;
  wm::ConvertPointFromScreen(
      window_util::GetRootWindowAt(location_in_screen_coordinates),
      &point_in_root);
  ui::MouseEvent event(ui::ET_MOUSE_DRAGGED, pt, point_in_root,
                       ui::EventTimeForNow(), 0, 0);
  PointerDraggedOnButton(drag_and_drop_view, DRAG_AND_DROP, event);
  return true;
}

void ShelfView::EndDrag(bool cancel) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  if (drag_and_drop_shelf_id_.IsNull())
    return;

  views::View* drag_and_drop_view =
      view_model_->view_at(model_->ItemIndexByID(drag_and_drop_shelf_id_));
  PointerReleasedOnButton(drag_and_drop_view, DRAG_AND_DROP, cancel);

  // Either destroy the temporarily created item - or - make the item visible.
  if (drag_and_drop_item_pinned_ && cancel) {
    model_->UnpinAppWithID(drag_and_drop_shelf_id_.app_id);
  } else if (drag_and_drop_view) {
    if (cancel) {
      // When a hosted drag gets canceled, the item can remain in the same slot
      // and it might have moved within the bounds. In that case the item need
      // to animate back to its correct location.
      AnimateToIdealBounds();
    } else {
      drag_and_drop_view->SetSize(pre_drag_and_drop_size_);
    }
  }

  drag_and_drop_shelf_id_ = ShelfID();
}

void ShelfView::SwapButtons(views::View* button_to_swap, bool with_next) {
  if (!button_to_swap)
    return;
  // We don't allow reordering of buttons that aren't app buttons.
  if (button_to_swap->GetClassName() != ShelfAppButton::kViewClassName)
    return;

  // Find the index of the button to swap in the view model.
  int src_index = -1;
  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    if (view == button_to_swap) {
      src_index = i;
      break;
    }
  }

  const int target_index = with_next ? src_index + 1 : src_index - 1;
  const int first_swappable_index = std::max(first_visible_index_, 0);
  const int last_swappable_index = last_visible_index_;
  if (src_index == -1 || (target_index > last_swappable_index) ||
      (target_index < first_swappable_index)) {
    return;
  }

  // Only allow swapping two pinned apps or two unpinned apps.
  if (!SamePinState(model_->items()[src_index].type,
                    model_->items()[target_index].type)) {
    return;
  }

  // Swapping items in the model is sufficient, everything will then be
  // reflected in the views.
  model_->Move(src_index, target_index);
  AnimateToIdealBounds();
  // TODO(manucornet): Announce the swap to screen readers.
}

void ShelfView::PointerPressedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (drag_view_)
    return;

  if (IsShowingMenu())
    shelf_menu_model_adapter_->Cancel();

  int index = view_model_->GetIndexOfView(view);
  if (index == -1 || view_model_->view_size() < 1)
    return;  // View is being deleted, ignore request.

  // The home and back buttons should be handled separately.
  DCHECK(view != GetHomeButton() || view != GetBackButton());

  // Only when the repost event occurs on the same shelf item, we should ignore
  // the call in ShelfView::ButtonPressed(...).
  is_repost_event_on_same_item_ =
      IsRepostEvent(event) && (last_pressed_index_ == index);

  CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  drag_view_ = static_cast<ShelfAppButton*>(view);
  drag_origin_ = gfx::Point(event.x(), event.y());
  UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentUsage",
                            static_cast<ShelfAlignmentUmaEnumValue>(
                                shelf_->SelectValueForShelfAlignment(
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
                                    SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT)),
                            SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
}

void ShelfView::PointerDraggedOnButton(views::View* view,
                                       Pointer pointer,
                                       const ui::LocatedEvent& event) {
  if (CanPrepareForDrag(pointer, event))
    PrepareForDrag(pointer, event);

  if (drag_pointer_ == pointer)
    ContinueDrag(event);
}

void ShelfView::PointerReleasedOnButton(views::View* view,
                                        Pointer pointer,
                                        bool canceled) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  is_repost_event_on_same_item_ = false;

  if (canceled) {
    CancelDrag(-1);
  } else if (drag_pointer_ == pointer) {
    FinalizeRipOffDrag(false);
    drag_pointer_ = NONE;
    AnimateToIdealBounds();
  }
  // If the drag pointer is NONE, no drag operation is going on and the
  // drag_view can be released.
  if (drag_pointer_ == NONE)
    drag_view_ = nullptr;
}

void ShelfView::LayoutToIdealBounds() {
  if (bounds_animator_->IsAnimating()) {
    AnimateToIdealBounds();
    return;
  }

  CalculateIdealBounds();
  views::ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
  UpdateBackButton();
  LayoutAppListAndBackButtonHighlight();
  LayoutBackAndHomeButtons();
  LayoutOverflowButton();
  UpdateVisibleShelfItemBoundsUnion();
}

bool ShelfView::IsItemPinned(const ShelfItem& item) const {
  return item.type == TYPE_PINNED_APP || item.type == TYPE_BROWSER_SHORTCUT;
}

void ShelfView::OnTabletModeChanged() {
  OnBoundsChanged(GetBoundsInScreen());
}

void ShelfView::LayoutBackAndHomeButtons() {
  back_button_->SetBoundsRect(back_button_->ideal_bounds());
  home_button_->SetBoundsRect(home_button_->ideal_bounds());
}

void ShelfView::LayoutOverflowButton() const {
  int x = 0;
  int y = 0;
  if (last_visible_index_ != -1) {
    const int offset = ShelfConstants::overflow_button_margin();
    x = shelf_->PrimaryAxisValue(
        offset + view_model_->ideal_bounds(last_visible_index_).right(),
        offset + view_model_->ideal_bounds(last_visible_index_).x());
    y = shelf_->PrimaryAxisValue(
        offset + view_model_->ideal_bounds(last_visible_index_).y(),
        offset + view_model_->ideal_bounds(last_visible_index_).bottom());

    // Add button spacing to correctly position overflow button next to app
    // buttons.
    x = shelf_->PrimaryAxisValue(x + ShelfConstants::button_spacing(), x);
    y = shelf_->PrimaryAxisValue(y, y + ShelfConstants::button_spacing());
  }

  overflow_button_->SetBoundsRect(gfx::Rect(
      x, y, ShelfConstants::control_size(), ShelfConstants::control_size()));
}

void ShelfView::AnimateToIdealBounds() {
  CalculateIdealBounds();

  // Handle back and home separately.
  ShelfControlButton* back = GetBackButton();
  bounds_animator_->AnimateViewTo(back, back->ideal_bounds());
  ShelfControlButton* home = GetHomeButton();
  bounds_animator_->AnimateViewTo(home, home->ideal_bounds());
  if (home->border())
    home->SetBorder(views::NullBorder());

  for (int i = 0; i < view_model_->view_size(); ++i) {
    View* view = view_model_->view_at(i);
    bounds_animator_->AnimateViewTo(view, view_model_->ideal_bounds(i));
    // Now that the item animation starts, we have to make sure that the
    // padding of the first gets properly transferred to the new first item.
    if (view->border())
      view->SetBorder(views::NullBorder());
  }
  LayoutAppListAndBackButtonHighlight();
  LayoutOverflowButton();
  UpdateVisibleShelfItemBoundsUnion();
}

void ShelfView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view, std::unique_ptr<gfx::AnimationDelegate>(
                new FadeInAnimationDelegate(view)));
}

void ShelfView::PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event) {
  DCHECK(!dragging());
  DCHECK(drag_view_);
  drag_pointer_ = pointer;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);
  drag_scroll_dir_ = 0;

  if (start_drag_index_ == -1) {
    CancelDrag(-1);
    return;
  }

  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);

  drag_view_->OnDragStarted(&event);
}

void ShelfView::ContinueDrag(const ui::LocatedEvent& event) {
  DCHECK(dragging());
  DCHECK(drag_view_);
  DCHECK_NE(-1, view_model_->GetIndexOfView(drag_view_));

  // If this is not a drag and drop host operation and not the app list item,
  // check if the item got ripped off the shelf - if it did we are done.
  if (drag_and_drop_shelf_id_.IsNull() &&
      RemovableByRipOff(view_model_->GetIndexOfView(drag_view_)) !=
          NOT_REMOVABLE &&
      HandleRipOffDrag(event)) {
    drag_scroll_dir_ = 0;
    scrolling_timer_.Stop();
    speed_up_drag_scrolling_.Stop();
    return;
  }

  // Scroll the overflow bubble as the user drags near either end. There could
  // be nowhere to scroll to in that direction (for that matter, the overflow
  // bubble may not even span the screen), but for simplicity, ignore that
  // possibility here and just let ScrollForUserDrag repeatedly do nothing.
  if (is_overflow_mode()) {
    const gfx::Rect bubble_bounds =
        owner_overflow_bubble_->bubble_view()->GetBubbleBounds();
    gfx::Point screen_location(event.location());
    ConvertPointToScreen(drag_view_, &screen_location);
    const int primary_coordinate =
        shelf_->PrimaryAxisValue(screen_location.x(), screen_location.y());

    int new_drag_scroll_dir = 0;
    if (primary_coordinate <
        shelf_->PrimaryAxisValue(bubble_bounds.x(), bubble_bounds.y()) +
            kScrollTriggerBoundsInsetDips) {
      new_drag_scroll_dir = -1;
    } else if (primary_coordinate >
               shelf_->PrimaryAxisValue(bubble_bounds.right(),
                                        bubble_bounds.bottom()) -
                   kScrollTriggerBoundsInsetDips) {
      new_drag_scroll_dir = 1;
    }

    if (new_drag_scroll_dir != drag_scroll_dir_) {
      drag_scroll_dir_ = new_drag_scroll_dir;
      scrolling_timer_.Stop();
      speed_up_drag_scrolling_.Stop();
      if (new_drag_scroll_dir != 0) {
        scrolling_timer_.Start(
            FROM_HERE, kDragScrollInterval,
            base::BindRepeating(
                &ShelfView::ScrollForUserDrag, base::Unretained(this),
                new_drag_scroll_dir * kDragSlowScrollDeltaDips));
        speed_up_drag_scrolling_.Start(FROM_HERE, kDragScrollSpeedIncreaseDelay,
                                       this, &ShelfView::SpeedUpDragScrolling);
      }
    }
  }

  gfx::Point drag_point(event.location());
  ConvertPointToTarget(drag_view_, this, &drag_point);
  MoveDragViewTo(shelf_->PrimaryAxisValue(drag_point.x() - drag_origin_.x(),
                                          drag_point.y() - drag_origin_.y()));
}

void ShelfView::ScrollForUserDrag(int offset) {
  DCHECK(dragging());
  DCHECK(drag_view_);
  DCHECK(is_overflow_mode());

  const gfx::Point position(drag_view_->origin());
  const int new_primary_coordinate =
      shelf_->IsHorizontalAlignment()
          ? position.x() +
                owner_overflow_bubble_->bubble_view()->ScrollByXOffset(
                    offset,
                    /*animate=*/false)
          : position.y() +
                owner_overflow_bubble_->bubble_view()->ScrollByYOffset(
                    offset,
                    /*animate=*/false);
  bounds_animator_->StopAnimatingView(drag_view_);
  MoveDragViewTo(new_primary_coordinate);
}

void ShelfView::SpeedUpDragScrolling() {
  DCHECK(dragging());
  DCHECK(drag_view_);
  DCHECK(is_overflow_mode());

  scrolling_timer_.Start(
      FROM_HERE, kDragScrollInterval,
      base::BindRepeating(&ShelfView::ScrollForUserDrag, base::Unretained(this),
                          drag_scroll_dir_ * kDragFastScrollDeltaDips));
}

void ShelfView::MoveDragViewTo(int primary_axis_coordinate) {
  const int current_index = view_model_->GetIndexOfView(drag_view_);
  const std::pair<int, int> indices(GetDragRange(current_index));
  if (shelf_->IsHorizontalAlignment()) {
    int x = GetMirroredXWithWidthInView(primary_axis_coordinate,
                                        drag_view_->width());
    x = std::max(view_model_->ideal_bounds(indices.first).x(), x);
    x = std::min(view_model_->ideal_bounds(indices.second).right() -
                     view_model_->ideal_bounds(current_index).width(),
                 x);
    if (drag_view_->x() == x)
      return;
    drag_view_->SetX(x);
  } else {
    int y = std::max(view_model_->ideal_bounds(indices.first).y(),
                     primary_axis_coordinate);
    y = std::min(view_model_->ideal_bounds(indices.second).bottom() -
                     view_model_->ideal_bounds(current_index).height(),
                 y);
    if (drag_view_->y() == y)
      return;
    drag_view_->SetY(y);
  }

  int target_index = views::ViewModelUtils::DetermineMoveIndex(
      *view_model_, drag_view_, shelf_->IsHorizontalAlignment(),
      drag_view_->x(), drag_view_->y());
  target_index =
      std::min(indices.second, std::max(target_index, indices.first));

  if (target_index == current_index)
    return;

  // Change the model, the ShelfItemMoved() callback will handle the
  // |view_model_| update.
  model_->Move(current_index, target_index);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void ShelfView::EndDragOnOtherShelf(bool cancel) {
  if (is_overflow_mode()) {
    main_shelf_->EndDrag(cancel);
  } else {
    DCHECK(overflow_bubble_->IsShowing());
    overflow_bubble_->bubble_view()->shelf_view()->EndDrag(cancel);
  }
}

bool ShelfView::HandleRipOffDrag(const ui::LocatedEvent& event) {
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);
  std::string dragged_app_id = model_->items()[current_index].id.app_id;

  gfx::Point screen_location = event.root_location();
  ::wm::ConvertPointToScreen(GetWidget()->GetNativeWindow()->GetRootWindow(),
                             &screen_location);

  // To avoid ugly forwards and backwards flipping we use different constants
  // for ripping off / re-inserting the items.
  if (dragged_off_shelf_) {
    // If the shelf/overflow bubble bounds contains |screen_location| we insert
    // the item back into the shelf.
    if (GetBoundsForDragInsertInScreen().Contains(screen_location)) {
      if (dragged_to_another_shelf_) {
        // During the dragging an item from Shelf to Overflow, it can enter here
        // directly because both are located very closely.
        EndDragOnOtherShelf(true /* cancel */);

        // Stops the animation of |drag_view_| and sets its bounds explicitly
        // because ContinueDrag() stops its animation. Without this, unexpected
        // bounds will be set.
        bounds_animator_->StopAnimatingView(drag_view_);
        int drag_view_index = view_model_->GetIndexOfView(drag_view_);
        drag_view_->SetBoundsRect(view_model_->ideal_bounds(drag_view_index));
        dragged_to_another_shelf_ = false;
      }
      // Destroy our proxy view item.
      DestroyDragIconProxy();
      // Re-insert the item and return simply false since the caller will handle
      // the move as in any normal case.
      dragged_off_shelf_ = false;
      drag_view_->layer()->SetOpacity(1.0f);
      // The size of Overflow bubble should be updated immediately when an item
      // is re-inserted.
      if (is_overflow_mode())
        PreferredSizeChanged();
      return false;
    } else if (is_overflow_mode() &&
               main_shelf_->GetBoundsForDragInsertInScreen().Contains(
                   screen_location)) {
      // The item was dragged from the overflow shelf to the main shelf.
      if (!dragged_to_another_shelf_) {
        dragged_to_another_shelf_ = true;
        drag_image_->SetOpacity(1.0f);
        main_shelf_->StartDrag(dragged_app_id, screen_location);
      } else {
        main_shelf_->Drag(screen_location);
      }
    } else if (!is_overflow_mode() && overflow_bubble_ &&
               overflow_bubble_->IsShowing() &&
               overflow_bubble_->bubble_view()
                   ->shelf_view()
                   ->GetBoundsForDragInsertInScreen()
                   .Contains(screen_location)) {
      // The item was dragged from the main shelf to the overflow shelf.
      if (!dragged_to_another_shelf_) {
        dragged_to_another_shelf_ = true;
        drag_image_->SetOpacity(1.0f);
        overflow_bubble_->bubble_view()->shelf_view()->StartDrag(
            dragged_app_id, screen_location);
      } else {
        overflow_bubble_->bubble_view()->shelf_view()->Drag(screen_location);
      }
    } else if (dragged_to_another_shelf_) {
      // Makes the |drag_image_| partially disappear again.
      dragged_to_another_shelf_ = false;
      drag_image_->SetOpacity(kDraggedImageOpacity);

      EndDragOnOtherShelf(true /* cancel */);
      if (!is_overflow_mode()) {
        // During dragging, the position of the dragged item is moved to the
        // back. If the overflow bubble is showing, a copy of the dragged item
        // will appear at the end of the overflow shelf. Decrement the last
        // visible index of the overflow shelf to hide this copy.
        overflow_bubble_->bubble_view()->shelf_view()->last_visible_index_--;
      }

      bounds_animator_->StopAnimatingView(drag_view_);
      int drag_view_index = view_model_->GetIndexOfView(drag_view_);
      drag_view_->SetBoundsRect(view_model_->ideal_bounds(drag_view_index));
    }
    // Move our proxy view item.
    UpdateDragIconProxy(screen_location);
    return true;
  }

  // Mark the item as dragged off the shelf if the drag distance exceeds
  // |kRipOffDistance|, or if it's dragged between the main and overflow shelf.
  int delta = CalculateShelfDistance(screen_location);
  bool dragged_off_shelf = delta > kRipOffDistance;
  dragged_off_shelf |=
      (is_overflow_mode() &&
       main_shelf_->GetBoundsForDragInsertInScreen().Contains(screen_location));
  dragged_off_shelf |= (!is_overflow_mode() && overflow_bubble_ &&
                        overflow_bubble_->IsShowing() &&
                        overflow_bubble_->bubble_view()
                            ->shelf_view()
                            ->GetBoundsForDragInsertInScreen()
                            .Contains(screen_location));

  if (dragged_off_shelf) {
    // Create a proxy view item which can be moved anywhere.
    CreateDragIconProxy(event.root_location(), drag_view_->GetImage(),
                        drag_view_, gfx::Vector2d(0, 0),
                        kDragAndDropProxyScale);
    drag_view_->layer()->SetOpacity(0.0f);
    dragged_off_shelf_ = true;
    if (RemovableByRipOff(current_index) == REMOVABLE) {
      // Move the item to the back and hide it. ShelfItemMoved() callback will
      // handle the |view_model_| update and call AnimateToIdealBounds().
      if (current_index != model_->item_count() - 1) {
        model_->Move(current_index, model_->item_count() - 1);
        StartFadeInLastVisibleItem();

        // During dragging, the position of the dragged item is moved to the
        // back. If the overflow bubble is showing, a copy of the dragged item
        // will appear at the end of the overflow shelf. Decrement the last
        // visible index of the overflow shelf to hide this copy.
        if (overflow_bubble_ && overflow_bubble_->IsShowing())
          overflow_bubble_->bubble_view()->shelf_view()->last_visible_index_--;
      } else if (is_overflow_mode()) {
        // Overflow bubble should be shrunk when an item is ripped off.
        PreferredSizeChanged();
      }
      // Make the item partially disappear to show that it will get removed if
      // dropped.
      drag_image_->SetOpacity(kDraggedImageOpacity);
    }
    return true;
  }
  return false;
}

void ShelfView::FinalizeRipOffDrag(bool cancel) {
  if (!dragged_off_shelf_)
    return;
  // Make sure we do not come in here again.
  dragged_off_shelf_ = false;

  // Coming here we should always have a |drag_view_|.
  DCHECK(drag_view_);
  int current_index = view_model_->GetIndexOfView(drag_view_);
  // If the view isn't part of the model anymore (|current_index| == -1), a sync
  // operation must have removed it. In that case we shouldn't change the model
  // and only delete the proxy image.
  if (current_index == -1) {
    DestroyDragIconProxy();
    return;
  }

  // Set to true when the animation should snap back to where it was before.
  bool snap_back = false;
  // Items which cannot be dragged off will be handled as a cancel.
  if (!cancel) {
    if (dragged_to_another_shelf_) {
      dragged_to_another_shelf_ = false;
      EndDragOnOtherShelf(false /* cancel */);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (RemovableByRipOff(current_index) != REMOVABLE) {
      // Make sure we do not try to remove un-removable items like items which
      // were not pinned or have to be always there.
      cancel = true;
      snap_back = true;
    } else {
      // Make sure the item stays invisible upon removal.
      drag_view_->SetVisible(false);
      model_->UnpinAppWithID(model_->items()[current_index].id.app_id);
    }
  }
  if (cancel || snap_back) {
    if (dragged_to_another_shelf_) {
      dragged_to_another_shelf_ = false;
      // Other shelf handles revert of dragged item.
      EndDragOnOtherShelf(false /* true */);
      drag_view_->layer()->SetOpacity(1.0f);
    } else if (!cancelling_drag_model_changed_) {
      // Only do something if the change did not come through a model change.
      gfx::Rect drag_bounds = drag_image_->GetBoundsInScreen();
      gfx::Point relative_to = GetBoundsInScreen().origin();
      gfx::Rect target(
          gfx::PointAtOffsetFromOrigin(drag_bounds.origin() - relative_to),
          drag_bounds.size());
      drag_view_->SetBoundsRect(target);
      // Hide the status from the active item since we snap it back now. Upon
      // animation end the flag gets cleared if |snap_back_from_rip_off_view_|
      // is set.
      snap_back_from_rip_off_view_ = drag_view_;
      drag_view_->AddState(ShelfAppButton::STATE_HIDDEN);
      // When a canceling drag model is happening, the view model is diverged
      // from the menu model and movements / animations should not be done.
      model_->Move(current_index, start_drag_index_);
      AnimateToIdealBounds();
    }
    drag_view_->layer()->SetOpacity(1.0f);
  }
  DestroyDragIconProxy();
}

ShelfView::RemovableState ShelfView::RemovableByRipOff(int index) const {
  DCHECK(index >= 0 && index < model_->item_count());
  ShelfItemType type = model_->items()[index].type;
  if (type == TYPE_DIALOG)
    return NOT_REMOVABLE;

  if (model_->items()[index].pinned_by_policy)
    return NOT_REMOVABLE;

  // Note: Only pinned app shortcuts can be removed!
  const std::string& app_id = model_->items()[index].id.app_id;
  return (type == TYPE_PINNED_APP && model_->IsAppPinned(app_id)) ? REMOVABLE
                                                                  : DRAGGABLE;
}

bool ShelfView::SameDragType(ShelfItemType typea, ShelfItemType typeb) const {
  switch (typea) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
      return (typeb == TYPE_PINNED_APP || typeb == TYPE_BROWSER_SHORTCUT);
    case TYPE_APP:
    case TYPE_DIALOG:
      return typeb == typea;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set.";
      return false;
  }
  NOTREACHED();
  return false;
}

bool ShelfView::ShouldFocusOut(bool reverse, views::View* button) {
  // The logic here seems backwards, but is actually correct. For instance if
  // the ShelfView's internal focus cycling logic attemmpts to focus the first
  // child (e.g. home button) after hitting Tab, we intercept that and
  // instead, advance through to the status area.
  return (reverse && button == FindLastFocusableChild()) ||
         (!reverse && button == FindFirstFocusableChild());
}

std::pair<int, int> ShelfView::GetDragRange(int index) {
  int min_index = -1;
  int max_index = -1;
  ShelfItemType type = model_->items()[index].type;
  for (int i = 0; i < model_->item_count(); ++i) {
    if (SameDragType(model_->items()[i].type, type)) {
      if (min_index == -1)
        min_index = i;
      max_index = i;
    }
  }
  min_index =
      std::max(min_index, is_overflow_mode() ? first_visible_index_ : 0);
  max_index = std::min(max_index, last_visible_index_);
  return std::pair<int, int>(min_index, max_index);
}

void ShelfView::OnFadeOutAnimationEnded() {
  AnimateToIdealBounds();
  StartFadeInLastVisibleItem();
}

void ShelfView::StartFadeInLastVisibleItem() {
  // If overflow button is visible and there is a valid new last item, fading
  // the new last item in after sliding animation is finished.
  if (overflow_button_->GetVisible() && last_visible_index_ >= 0) {
    views::View* last_visible_view = view_model_->view_at(last_visible_index_);
    last_visible_view->layer()->SetOpacity(0);
    bounds_animator_->SetAnimationDelegate(
        last_visible_view,
        std::unique_ptr<gfx::AnimationDelegate>(
            new StartFadeAnimationDelegate(this, last_visible_view)));
  }
}

gfx::Rect ShelfView::GetMenuAnchorRect(const views::View& source,
                                       const gfx::Point& location,
                                       bool context_menu) const {
  // Application menus for items are anchored on the icon bounds.
  if (ShelfItemForView(&source) || !context_menu)
    return source.GetBoundsInScreen();

  const gfx::Rect shelf_bounds_in_screen =
      is_overflow_mode()
          ? owner_overflow_bubble_->bubble_view()->GetBubbleBounds()
          : GetBoundsInScreen();
  gfx::Point origin;
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      origin = gfx::Point(location.x(), shelf_bounds_in_screen.y());
      break;
    case SHELF_ALIGNMENT_LEFT:
      origin = gfx::Point(shelf_bounds_in_screen.right(), location.y());
      break;
    case SHELF_ALIGNMENT_RIGHT:
      origin = gfx::Point(shelf_bounds_in_screen.x(), location.y());
      break;
  }
  return gfx::Rect(origin, gfx::Size());
}

void ShelfView::AnnounceShelfAlignment() {
  base::string16 announcement;
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_BOTTOM);
      break;
    case SHELF_ALIGNMENT_LEFT:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_LEFT);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_ALIGNMENT_RIGHT);
      break;
  }
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void ShelfView::AnnounceShelfAutohideBehavior() {
  base::string16 announcement;
  switch (shelf_->auto_hide_behavior()) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_AUTO_HIDE);
      break;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_ALWAYS_SHOWN);
      break;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      announcement = l10n_util::GetStringUTF16(IDS_SHELF_STATE_ALWAYS_HIDDEN);
      break;
  }
  announcement_view_->GetViewAccessibility().OverrideName(announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

gfx::Rect ShelfView::GetBoundsForDragInsertInScreen() {
  gfx::Size preferred_size;
  if (is_overflow_mode()) {
    DCHECK(owner_overflow_bubble_);
    gfx::Rect bubble_bounds =
        owner_overflow_bubble_->bubble_view()->GetBubbleBounds();
    preferred_size = bubble_bounds.size();
  } else {
    const int last_button_index = view_model_->view_size() - 1;
    gfx::Rect last_button_bounds =
        view_model_->view_at(last_button_index)->bounds();
    if (overflow_button_->GetVisible()) {
      // When overflow button is visible, last_button_bounds should be
      // overflow button's bounds.
      last_button_bounds = overflow_button_->bounds();
    }

    if (shelf_->IsHorizontalAlignment()) {
      preferred_size =
          gfx::Size(last_button_bounds.right(), ShelfConstants::shelf_size());
    } else {
      preferred_size =
          gfx::Size(ShelfConstants::shelf_size(), last_button_bounds.bottom());
    }
  }
  gfx::Point origin(GetMirroredXWithWidthInView(0, preferred_size.width()), 0);

  // In overflow mode, we should use OverflowBubbleView as a source for
  // converting |origin| to screen coordinates. When a scroll operation is
  // occurred in OverflowBubble, the bounds of ShelfView in OverflowBubble can
  // be changed.
  if (is_overflow_mode())
    ConvertPointToScreen(owner_overflow_bubble_->bubble_view(), &origin);
  else
    ConvertPointToScreen(this, &origin);

  return gfx::Rect(origin, preferred_size);
}

int ShelfView::CancelDrag(int modified_index) {
  drag_scroll_dir_ = 0;
  scrolling_timer_.Stop();
  speed_up_drag_scrolling_.Stop();

  FinalizeRipOffDrag(true);
  if (!drag_view_)
    return modified_index;
  bool was_dragging = dragging();
  int drag_view_index = view_model_->GetIndexOfView(drag_view_);
  drag_pointer_ = NONE;
  drag_view_ = nullptr;
  if (drag_view_index == modified_index) {
    // The view that was being dragged is being modified. Don't do anything.
    return modified_index;
  }
  if (!was_dragging)
    return modified_index;

  // Restore previous position, tracking the position of the modified view.
  bool at_end = modified_index == view_model_->view_size();
  views::View* modified_view = (modified_index >= 0 && !at_end)
                                   ? view_model_->view_at(modified_index)
                                   : nullptr;
  model_->Move(drag_view_index, start_drag_index_);

  // If the modified view will be at the end of the list, return the new end of
  // the list.
  if (at_end)
    return view_model_->view_size();
  return modified_view ? view_model_->GetIndexOfView(modified_view) : -1;
}

void ShelfView::OnGestureEvent(ui::GestureEvent* event) {
  // Convert the event location from current view to screen, since swiping up on
  // the shelf can open the fullscreen app list. Updating the bounds of the app
  // list during dragging is based on screen coordinate space.
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);
  event->set_location(location_in_screen);
  if (shelf_->ProcessGestureEvent(*event))
    event->StopPropagation();
  else if (is_overflow_mode()) {
    // If the event hasn't been processed yet and the overflow shelf is showing,
    // give the bubble a chance to process the event.
    if (main_shelf_->overflow_bubble()->bubble_view()->ProcessGestureEvent(
            *event)) {
      event->StopPropagation();
      return;
    }
  }
}

void ShelfView::OnMouseEvent(ui::MouseEvent* event) {
  gfx::Point location_in_screen(event->location());
  View::ConvertPointToScreen(this, &location_in_screen);

  switch (event->type()) {
    case ui::ET_MOUSEWHEEL:
      event->SetHandled();
      shelf_->ProcessMouseWheelEvent(*event->AsMouseWheelEvent());
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!event->IsOnlyLeftMouseButton()) {
        if (event->IsOnlyRightMouseButton()) {
          ShowContextMenuForViewImpl(this, location_in_screen,
                                     ui::MENU_SOURCE_MOUSE);
          event->SetHandled();
        }
        return;
      }

      FALLTHROUGH;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
      // Convert the event location from current view to screen, since dragging
      // the shelf by mouse can open the fullscreen app list. Updating the
      // bounds of the app list during dragging is based on screen coordinate
      // space.
      event->set_location(location_in_screen);

      event->SetHandled();
      shelf_->ProcessMouseEvent(*event->AsMouseEvent());
      break;
    default:
      break;
  }
}

void ShelfView::ShelfItemAdded(int model_index) {
  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    model_index = CancelDrag(model_index);
  }
  const ShelfItem& item(model_->items()[model_index]);
  views::View* view = CreateViewForItem(item);
  // Some item types are ignored and yield a null view. Ignore those.
  // TODO(manucornet): Remove this if statement once home and back are no
  // longer acceptable shelf item types in the model.
  if (!view)
    return;
  AddChildView(view);
  // Hide the view, it'll be made visible when the animation is done. Using
  // opacity 0 here to avoid messing with CalculateIdealBounds which touches
  // the view's visibility.
  view->layer()->SetOpacity(0);
  view_model_->Add(view, model_index);

  // Give the button its ideal bounds. That way if we end up animating the
  // button before this animation completes it doesn't appear at some random
  // spot (because it was in the middle of animating from 0,0 0x0 to its
  // target).
  CalculateIdealBounds();
  view->SetBoundsRect(view_model_->ideal_bounds(model_index));

  // The first animation moves all the views to their target position. |view|
  // is hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();
  if (model_index <= last_visible_index_) {
    bounds_animator_->SetAnimationDelegate(
        view, std::unique_ptr<gfx::AnimationDelegate>(
                  new StartFadeAnimationDelegate(this, view)));
  } else {
    // Undo the hiding if animation does not run.
    view->layer()->SetOpacity(1.0f);
  }
}

void ShelfView::ShelfItemRemoved(int model_index, const ShelfItem& old_item) {
  if (old_item.id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();

  views::View* view = view_model_->view_at(model_index);
  view_model_->Remove(model_index);

  {
    base::AutoReset<bool> cancelling_drag(&cancelling_drag_model_changed_,
                                          true);
    CancelDrag(-1);
  }

  // When the overflow bubble is visible, the overflow range needs to be set
  // before CalculateIdealBounds() gets called. Otherwise CalculateIdealBounds()
  // could trigger a ShelfItemChanged() by hiding the overflow bubble and
  // since the overflow bubble is not yet synced with the ShelfModel this
  // could cause a crash.
  if (overflow_bubble_ && overflow_bubble_->IsShowing()) {
    UpdateOverflowRange(overflow_bubble_->bubble_view()->shelf_view());
  }

  if (view->GetVisible()) {
    // The first animation fades out the view. When done we'll animate the rest
    // of the views to their target location.
    bounds_animator_->AnimateViewTo(view, view->bounds());
    bounds_animator_->SetAnimationDelegate(
        view, std::unique_ptr<gfx::AnimationDelegate>(
                  new FadeOutAnimationDelegate(this, view)));
  } else {
    // We don't need to show a fade out animation for invisible |view|. When an
    // item is ripped out from the shelf, its |view| is already invisible.
    AnimateToIdealBounds();
  }

  if (view == tooltip_.GetCurrentAnchorView())
    tooltip_.Close();
}

void ShelfView::ShelfItemChanged(int model_index, const ShelfItem& old_item) {
  const ShelfItem& item(model_->items()[model_index]);

  // Bail if the view and shelf sizes do not match. ShelfItemChanged may be
  // called here before ShelfItemAdded, due to ChromeLauncherController's
  // item initialization, which calls SetItem during ShelfItemAdded.
  if (static_cast<int>(model_->items().size()) != view_model_->view_size())
    return;

  if (old_item.type != item.type) {
    // Type changed, swap the views.
    model_index = CancelDrag(model_index);
    std::unique_ptr<views::View> old_view(view_model_->view_at(model_index));
    bounds_animator_->StopAnimatingView(old_view.get());
    // Removing and re-inserting a view in our view model will strip the ideal
    // bounds from the item. To avoid recalculation of everything the bounds
    // get remembered and restored after the insertion to the previous value.
    gfx::Rect old_ideal_bounds = view_model_->ideal_bounds(model_index);
    view_model_->Remove(model_index);
    views::View* new_view = CreateViewForItem(item);
    AddChildView(new_view);
    view_model_->Add(new_view, model_index);
    view_model_->set_ideal_bounds(model_index, old_ideal_bounds);

    new_view->SetBoundsRect(old_view->bounds());
    if (overflow_button_ && overflow_button_->GetVisible())
      AnimateToIdealBounds();
    else
      bounds_animator_->AnimateViewTo(new_view, old_ideal_bounds);

    // If an item is being pinned or unpinned, show the new status of the
    // shelf immediately so that the separator gets drawn as needed.
    if (old_item.type == TYPE_PINNED_APP || item.type == TYPE_PINNED_APP)
      AnimateToIdealBounds();
    return;
  }

  views::View* view = view_model_->view_at(model_index);
  switch (item.type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_DIALOG: {
      CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
      ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
      button->ReflectItemStatus(item);
      button->SetImage(item.image);
      button->SchedulePaint();
      break;
    }

    default:
      break;
  }
}

void ShelfView::ShelfItemMoved(int start_index, int target_index) {
  view_model_->Move(start_index, target_index);
  // When cancelling a drag due to a shelf item being added, the currently
  // dragged item is moved back to its initial position. AnimateToIdealBounds
  // will be called again when the new item is added to the |view_model_| but
  // at this time the |view_model_| is inconsistent with the |model_|.
  if (!cancelling_drag_model_changed_)
    AnimateToIdealBounds();
}

void ShelfView::ShelfItemDelegateChanged(const ShelfID& id,
                                         ShelfItemDelegate* old_delegate,
                                         ShelfItemDelegate* delegate) {
  if (id == context_menu_id_ && shelf_menu_model_adapter_)
    shelf_menu_model_adapter_->Cancel();
}

void ShelfView::ShelfItemStatusChanged(const ShelfID& id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0)
    return;

  const ShelfItem item = model_->items()[index];
  views::View* view = view_model_->view_at(index);
  CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
  // TODO(manucornet): Add a helper to get the button.
  ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
  button->ReflectItemStatus(item);
  button->SchedulePaint();
}

void ShelfView::OnShelfAlignmentChanged(aura::Window* root_window) {
  LayoutToIdealBounds();
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (i >= first_visible_index_ && i <= last_visible_index_)
      view_model_->view_at(i)->Layout();
  }
  tooltip_.Close();
  if (overflow_bubble_)
    overflow_bubble_->Hide();
  // For crbug.com/587931, because HomeButton layout logic is in OnPaint.
  HomeButton* home_button = GetHomeButton();
  if (home_button)
    home_button->SchedulePaint();

  AnnounceShelfAlignment();
}

void ShelfView::OnShelfAutoHideBehaviorChanged(aura::Window* root_window) {
  AnnounceShelfAutohideBehavior();
}

void ShelfView::AfterItemSelected(const ShelfItem& item,
                                  views::Button* sender,
                                  std::unique_ptr<ui::Event> event,
                                  views::InkDrop* ink_drop,
                                  ShelfAction action,
                                  ShelfItemDelegate::AppMenuItems menu_items) {
  item_awaiting_response_ = ShelfID();
  shelf_button_pressed_metric_tracker_.ButtonPressed(*event, sender, action);

  // Record AppList metric for any action considered an app launch.
  if ((action == SHELF_ACTION_NEW_WINDOW_CREATED ||
       action == SHELF_ACTION_WINDOW_ACTIVATED) &&
      Shell::Get()->app_list_controller()) {
    Shell::Get()->app_list_controller()->RecordShelfAppLaunched(
        recorded_app_list_view_state_, recorded_home_launcher_shown_);
  }

  // The app list handles its own ink drop effect state changes.
  if (action == SHELF_ACTION_APP_LIST_DISMISSED) {
    ink_drop->SnapToActivated();
    ink_drop->AnimateToState(views::InkDropState::HIDDEN);
  } else if (action != SHELF_ACTION_APP_LIST_SHOWN) {
    if (action != SHELF_ACTION_NEW_WINDOW_CREATED && menu_items.size() > 1) {
      // Show the app menu with 2 or more items, if no window was created.
      ink_drop->AnimateToState(views::InkDropState::ACTIVATED);
      context_menu_id_ = item.id;
      ShowMenu(std::make_unique<ShelfApplicationMenuModel>(
                   item.title, std::move(menu_items),
                   model_->GetShelfItemDelegate(item.id)),
               sender, gfx::Point(), /*context_menu=*/false,
               ui::GetMenuSourceTypeForEvent(*event));
      shelf_->UpdateVisibilityState();
    } else {
      ink_drop->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
    }
  }
  scoped_root_window_for_new_windows_.reset();
}

void ShelfView::ShowShelfContextMenu(
    const ShelfID& shelf_id,
    const gfx::Point& point,
    views::View* source,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  context_menu_id_ = shelf_id;
  if (!model) {
    const int64_t display_id = GetDisplayIdForView(this);
    model = std::make_unique<ShelfContextMenuModel>(nullptr, display_id);
  }
  ShowMenu(std::move(model), source, point, /*context_menu=*/true, source_type);
}

void ShelfView::ShowContextMenuForViewImpl(views::View* source,
                                           const gfx::Point& point,
                                           ui::MenuSourceType source_type) {
  // Prevent concurrent requests that may show application or context menus.
  const ShelfItem* item = ShelfItemForView(source);
  if (!item_awaiting_response_.IsNull()) {
    if (item && item->id != item_awaiting_response_) {
      static_cast<views::Button*>(source)->AnimateInkDrop(
          views::InkDropState::DEACTIVATED, nullptr);
    }
    return;
  }
  last_pressed_index_ = -1;
  if (!item || !model_->GetShelfItemDelegate(item->id)) {
    ShowShelfContextMenu(ShelfID(), point, source, source_type, nullptr);
    return;
  }

  item_awaiting_response_ = item->id;
  const int64_t display_id = GetDisplayIdForView(this);
  model_->GetShelfItemDelegate(item->id)->GetContextMenu(
      display_id, base::BindOnce(&ShelfView::ShowShelfContextMenu,
                                 weak_factory_.GetWeakPtr(), item->id, point,
                                 source, source_type));
}

void ShelfView::ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                         views::View* source,
                         const gfx::Point& click_point,
                         bool context_menu,
                         ui::MenuSourceType source_type) {
  // Delayed callbacks to show context and application menus may conflict; hide
  // the old menu before showing a new menu in that case.
  if (IsShowingMenu())
    shelf_menu_model_adapter_->Cancel();

  item_awaiting_response_ = ShelfID();
  if (menu_model->GetItemCount() == 0)
    return;
  menu_owner_ = source;

  closing_event_time_ = base::TimeTicks();

  // NOTE: If you convert to HAS_MNEMONICS be sure to update menu building code.
  int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT;
  if (context_menu) {
    run_types |=
        views::MenuRunner::CONTEXT_MENU | views::MenuRunner::FIXED_ANCHOR;
  }

  const ShelfItem* item = ShelfItemForView(source);
  // Only selected shelf items with context menu opened can be dragged.
  if (context_menu && item && ShelfButtonIsInDrag(item->type, source) &&
      source_type == ui::MenuSourceType::MENU_SOURCE_TOUCH) {
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;
  }

  shelf_menu_model_adapter_ = std::make_unique<ShelfMenuModelAdapter>(
      item ? item->id.app_id : std::string(), std::move(menu_model), source,
      source_type,
      base::BindOnce(&ShelfView::OnMenuClosed, base::Unretained(this), source),
      IsTabletModeEnabled());
  shelf_menu_model_adapter_->Run(
      GetMenuAnchorRect(*source, click_point, context_menu),
      shelf_->IsHorizontalAlignment() ? views::MenuAnchorPosition::kBubbleAbove
                                      : views::MenuAnchorPosition::kBubbleLeft,
      run_types);
}

void ShelfView::OnMenuClosed(views::View* source) {
  menu_owner_ = nullptr;
  context_menu_id_ = ShelfID();

  closing_event_time_ = shelf_menu_model_adapter_->GetClosingEventTime();

  const ShelfItem* item = ShelfItemForView(source);
  if (item)
    static_cast<ShelfAppButton*>(source)->OnMenuClosed();

  shelf_menu_model_adapter_.reset();

  const bool is_in_drag = item && ShelfButtonIsInDrag(item->type, source);
  // Update the shelf visibility since auto-hide or alignment might have
  // changes, but don't update if shelf item is being dragged. Since shelf
  // should be kept as visible during shelf item drag even menu is closed.
  if (!is_in_drag)
    shelf_->UpdateVisibilityState();
}

void ShelfView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
  shelf_->NotifyShelfIconPositionsChanged();
  PreferredSizeChanged();

  if (shelf_->is_tablet_mode_animation_running()) {
    float opacity = 0.f;
    const gfx::SlideAnimation* animation =
        bounds_animator_->GetAnimationForView(GetBackButton());
    if (animation)
      opacity = static_cast<float>(animation->GetCurrentValue());
    if (!IsTabletModeEnabled())
      opacity = 1.f - opacity;

    GetBackButton()->layer()->SetOpacity(opacity);
    GetBackButton()->SetFocusBehavior(FocusBehavior::ALWAYS);
  }
}

void ShelfView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  shelf_->set_is_tablet_mode_animation_running(false);

  if (snap_back_from_rip_off_view_ && animator == bounds_animator_.get()) {
    if (!animator->IsAnimating(snap_back_from_rip_off_view_)) {
      // Coming here the animation of the ShelfAppButton is finished and the
      // previously hidden status can be shown again. Since the button itself
      // might have gone away or changed locations we check that the button
      // is still in the shelf and show its status again.
      for (int index = 0; index < view_model_->view_size(); index++) {
        views::View* view = view_model_->view_at(index);
        if (view == snap_back_from_rip_off_view_) {
          CHECK_EQ(ShelfAppButton::kViewClassName, view->GetClassName());
          ShelfAppButton* button = static_cast<ShelfAppButton*>(view);
          button->ClearState(ShelfAppButton::STATE_HIDDEN);
          break;
        }
      }
      snap_back_from_rip_off_view_ = nullptr;
    }
  }

  UpdateBackButton();
}

bool ShelfView::IsRepostEvent(const ui::Event& event) {
  if (closing_event_time_.is_null())
    return false;

  // If the current (press down) event is a repost event, the time stamp of
  // these two events should be the same.
  return closing_event_time_ == event.time_stamp();
}

const ShelfItem* ShelfView::ShelfItemForView(const views::View* view) const {
  const int view_index = view_model_->GetIndexOfView(view);
  return (view_index < 0) ? nullptr : &(model_->items()[view_index]);
}

int ShelfView::CalculateShelfDistance(const gfx::Point& coordinate) const {
  const gfx::Rect bounds = GetBoundsInScreen();
  int distance = shelf_->SelectValueForShelfAlignment(
      bounds.y() - coordinate.y(), coordinate.x() - bounds.right(),
      bounds.x() - coordinate.x());
  return distance > 0 ? distance : 0;
}

bool ShelfView::CanPrepareForDrag(Pointer pointer,
                                  const ui::LocatedEvent& event) {
  // Bail if dragging has already begun, or if no item has been pressed.
  if (dragging() || !drag_view_)
    return false;

  // Dragging only begins once the pointer has travelled a minimum distance.
  if ((std::abs(event.x() - drag_origin_.x()) < kMinimumDragDistance) &&
      (std::abs(event.y() - drag_origin_.y()) < kMinimumDragDistance)) {
    return false;
  }

  return true;
}

void ShelfView::UpdateBackButton() {
  const bool virtual_keyboard_visible =
      Shell::Get()->system_tray_model()->virtual_keyboard()->visible();
  gfx::Transform rotation;
  // Rotate the back button when virtual keyboard is visible.
  if (virtual_keyboard_visible)
    rotation.Rotate(270.0);
  GetBackButton()->layer()->SetOpacity(IsTabletModeEnabled() ? 1.f : 0.f);
  GetBackButton()->layer()->SetTransform(
      TransformAboutPivot(GetBackButton()->GetCenterPoint(), rotation));
  GetBackButton()->SetFocusBehavior(
      IsTabletModeEnabled() ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
}

void ShelfView::SetDragImageBlur(const gfx::Size& size, int blur_radius) {
  drag_image_->SetPaintToLayer();
  drag_image_->layer()->SetFillsBoundsOpaquely(false);
  const uint32_t radius = std::round(size.width() / 2.f);
  drag_image_->layer()->SetRoundedCornerRadius(
      {radius, radius, radius, radius});
  drag_image_->layer()->SetBackgroundBlur(blur_radius);
}

}  // namespace ash
