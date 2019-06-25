// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_actions_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/numerics/ranges.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// Image buttons.
constexpr int kImageButtonSizeDip = 40;
constexpr int kActionButtonBetweenSpacing = 8;
// Button hover color, Google Grey 8%.
constexpr SkColor kButtonHoverColor = SkColorSetA(gfx::kGoogleGrey900, 0x14);

}  // namespace

// SearchResultImageButton renders the button defined by SearchResult::Action.
class SearchResultImageButton : public views::ImageButton {
 public:
  SearchResultImageButton(SearchResultActionsView* parent,
                          const SearchResult::Action& action);
  ~SearchResultImageButton() override {}

  // views::View overrides:
  void OnFocus() override;
  void OnBlur() override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::Button overrides:
  void StateChanged(ButtonState old_state) override;

  // views::InkDropHost overrides:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  // Updates the button visibility upon state change of the button or the
  // search result view associated with it.
  void UpdateOnStateChanged();

 private:
  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  void SetButtonImage(const gfx::ImageSkia& source, int icon_dimension);

  int GetInkDropRadius() const;
  const char* GetClassName() const override;

  SearchResultActionsView* parent_;
  const bool visible_on_hover_;
  bool to_be_activate_by_long_press_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchResultImageButton);
};

SearchResultImageButton::SearchResultImageButton(
    SearchResultActionsView* parent,
    const SearchResult::Action& action)
    : ImageButton(parent),
      parent_(parent),
      visible_on_hover_(action.visible_on_hover) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Avoid drawing default dashed focus and draw customized focus in
  // OnPaintBackground();
  SetFocusPainter(nullptr);
  SetInkDropMode(InkDropMode::ON);

  SetPreferredSize({kImageButtonSizeDip, kImageButtonSizeDip});
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  SetButtonImage(action.image,
                 AppListConfig::instance().search_list_icon_dimension());

  SetAccessibleName(action.tooltip_text);

  SetTooltipText(action.tooltip_text);

  SetVisible(!visible_on_hover_);
}

void SearchResultImageButton::OnFocus() {
  parent_->ActionButtonStateChanged();
  SchedulePaint();
  if (GetVisible())
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void SearchResultImageButton::OnBlur() {
  parent_->ActionButtonStateChanged();
  SchedulePaint();
}

void SearchResultImageButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      to_be_activate_by_long_press_ = true;
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      if (to_be_activate_by_long_press_) {
        NotifyClick(*event);
        SetState(STATE_NORMAL);
        to_be_activate_by_long_press_ = false;
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled())
    Button::OnGestureEvent(event);
}

void SearchResultImageButton::StateChanged(
    views::Button::ButtonState old_state) {
  parent_->ActionButtonStateChanged();
}

std::unique_ptr<views::InkDrop> SearchResultImageButton::CreateInkDrop() {
  return CreateDefaultFloodFillInkDropImpl();
}

std::unique_ptr<views::InkDropRipple>
SearchResultImageButton::CreateInkDropRipple() const {
  const gfx::Point center = GetLocalBounds().CenterPoint();
  const int ripple_radius = GetInkDropRadius();
  gfx::Rect bounds(center.x() - ripple_radius, center.y() - ripple_radius,
                   2 * ripple_radius, 2 * ripple_radius);
  constexpr SkColor ripple_color = SkColorSetA(gfx::kGoogleGrey900, 0x17);

  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), ripple_color, 1.0f);
}

std::unique_ptr<views::InkDropMask> SearchResultImageButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
}

std::unique_ptr<views::InkDropHighlight>
SearchResultImageButton::CreateInkDropHighlight() const {
  constexpr SkColor ripple_color = SkColorSetA(gfx::kGoogleGrey900, 0x12);
  return std::make_unique<views::InkDropHighlight>(
      gfx::PointF(GetLocalBounds().CenterPoint()),
      std::make_unique<views::CircleLayerDelegate>(ripple_color,
                                                   GetInkDropRadius()));
}

void SearchResultImageButton::UpdateOnStateChanged() {
  if (!visible_on_hover_)
    return;

  // Show button if the associated result row is hovered or selected, or one
  // of the action buttons is selected.
  SetVisible(parent_->IsSearchResultHoveredOrSelected() ||
             parent()->Contains(GetFocusManager()->GetFocusedView()));
}

void SearchResultImageButton::OnPaintBackground(gfx::Canvas* canvas) {
  if (HasFocus()) {
    cc::PaintFlags circle_flags;
    circle_flags.setAntiAlias(true);
    circle_flags.setColor(kButtonHoverColor);
    circle_flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(GetLocalBounds().CenterPoint(), GetInkDropRadius(),
                       circle_flags);
  }
}

void SearchResultImageButton::SetButtonImage(const gfx::ImageSkia& source,
                                             int icon_dimension) {
  SetImage(views::ImageButton::STATE_NORMAL,
           gfx::ImageSkiaOperations::CreateResizedImage(
               source, skia::ImageOperations::RESIZE_BEST,
               gfx::Size(icon_dimension, icon_dimension)));
}

int SearchResultImageButton::GetInkDropRadius() const {
  return width() / 2;
}

const char* SearchResultImageButton::GetClassName() const {
  return "SearchResultImageButton";
}

SearchResultActionsView::SearchResultActionsView(
    SearchResultActionsViewDelegate* delegate)
    : delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kActionButtonBetweenSpacing));
}

SearchResultActionsView::~SearchResultActionsView() {}

void SearchResultActionsView::SetActions(const SearchResult::Actions& actions) {
  buttons_.clear();
  RemoveAllChildViews(true);
  for (const auto& action : actions)
    CreateImageButton(action);
  PreferredSizeChanged();
}

bool SearchResultActionsView::IsValidActionIndex(size_t action_index) const {
  return action_index < children().size();
}

bool SearchResultActionsView::IsSearchResultHoveredOrSelected() const {
  return delegate_->IsSearchResultHoveredOrSelected();
}

void SearchResultActionsView::UpdateButtonsOnStateChanged() {
  for (SearchResultImageButton* button : buttons_)
    button->UpdateOnStateChanged();
}

void SearchResultActionsView::ActionButtonStateChanged() {
  UpdateButtonsOnStateChanged();
}

const char* SearchResultActionsView::GetClassName() const {
  return "SearchResultActionsView";
}

void SearchResultActionsView::CreateImageButton(
    const SearchResult::Action& action) {
  SearchResultImageButton* button = new SearchResultImageButton(this, action);
  AddChildView(button);
  buttons_.emplace_back(button);
}

void SearchResultActionsView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void SearchResultActionsView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (!delegate_)
    return;

  const int index = GetIndexOf(sender);
  DCHECK_NE(-1, index);
  delegate_->OnSearchResultActionActivated(index, event.flags());
}

}  // namespace app_list
