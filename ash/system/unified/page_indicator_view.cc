// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/page_indicator_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/skia_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kInkDropRadius = 3 * kUnifiedPageIndicatorButtonRadius;

constexpr SkColor kInkDropRippleColor =
    SkColorSetA(kUnifiedPageIndicatorButtonInkDropColor, 0xF);
constexpr SkColor kInkDropHighlightColor =
    SkColorSetA(kUnifiedPageIndicatorButtonInkDropColor, 0x14);

}  // namespace

// Button internally used in PageIndicatorView. Each button
// stores a page number which it switches to if pressed.
class PageIndicatorView::PageIndicatorButton : public views::Button,
                                               public views::ButtonListener {
 public:
  explicit PageIndicatorButton(UnifiedSystemTrayController* controller,
                               int page)
      : views::Button(this), controller_(controller), page_number_(page) {
    SetInkDropMode(InkDropMode::ON);
  }

  ~PageIndicatorButton() override {}

  void SetSelected(bool selected) {
    if (selected == selected_)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kInkDropRadius * 2, kInkDropRadius * 2);
  }

  // views::Button:
  const char* GetClassName() const override { return "PageIndicatorView"; }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::Rect rect(GetContentsBounds());

    SkColor current_color = selected_
                                ? kUnifiedPageIndicatorButtonColor
                                : SkColorSetA(kUnifiedPageIndicatorButtonColor,
                                              kUnifiedPageIndicatorButtonAlpha);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(current_color);
    canvas->DrawCircle(rect.CenterPoint(), kUnifiedPageIndicatorButtonRadius,
                       flags);
  }
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(controller_);
    controller_->HandlePageSwitchAction(page_number_);
  }

  bool selected() { return selected_; }

 protected:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        Button::CreateDefaultInkDropImpl();
    ink_drop->SetShowHighlightOnHover(true);
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), kInkDropRadius);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    gfx::Rect bounds(center.x() - kInkDropRadius, center.y() - kInkDropRadius,
                     2 * kInkDropRadius, 2 * kInkDropRadius);
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor, 1.0f);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(kInkDropHighlightColor,
                                                     kInkDropRadius));
  }

  void NotifyClick(const ui::Event& event) override {
    Button::NotifyClick(event);
    GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
  }

 private:
  bool selected_ = false;
  UnifiedSystemTrayController* const controller_;
  const int page_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PageIndicatorButton);
};

PageIndicatorView::PageIndicatorView(UnifiedSystemTrayController* controller,
                                     bool initially_expanded)
    : controller_(controller),
      model_(controller->model()->pagination_model()),
      expanded_amount_(initially_expanded ? 1 : 0),
      buttons_container_(new views::View) {
  SetVisible(initially_expanded);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  buttons_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets()));

  AddChildView(buttons_container_);

  TotalPagesChanged();

  DCHECK(model_);
  model_->AddObserver(this);
}

PageIndicatorView::~PageIndicatorView() {
  model_->RemoveObserver(this);
}

gfx::Size PageIndicatorView::CalculatePreferredSize() const {
  gfx::Size size = buttons_container_->GetPreferredSize();
  size.set_height(size.height() * expanded_amount_);
  return size;
}

void PageIndicatorView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  gfx::Size buttons_container_size(buttons_container_->GetPreferredSize());
  rect.ClampToCenteredSize(buttons_container_size);
  buttons_container_->SetBoundsRect(rect);
}

const char* PageIndicatorView::GetClassName() const {
  return "PageIndicatorView";
}

void PageIndicatorView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  SetVisible(expanded_amount > 0.0);
  expanded_amount_ = expanded_amount;
  InvalidateLayout();
  // TODO(amehfooz): Confirm animation curve with UX.
  layer()->SetOpacity(std::max(0., 6 * expanded_amount_ - 5.));
}

void PageIndicatorView::TotalPagesChanged() {
  DCHECK(model_);

  buttons_container_->RemoveAllChildViews(true);
  for (int i = 0; i < model_->total_pages(); ++i) {
    PageIndicatorButton* button = new PageIndicatorButton(controller_, i);
    button->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_PAGE_SWITCHER, base::FormatNumber(i + 1),
        base::FormatNumber(model_->total_pages())));
    button->SetSelected(i == model_->selected_page());
    buttons_container_->AddChildView(button);
  }
  buttons_container_->SetVisible(model_->total_pages() > 1);
  Layout();
}

PageIndicatorView::PageIndicatorButton* PageIndicatorView::GetButtonByIndex(
    int index) {
  return static_cast<PageIndicatorButton*>(
      buttons_container_->children().at(index));
}

void PageIndicatorView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  size_t total_children = buttons_container_->children().size();

  if (old_selected >= 0 && size_t{old_selected} < total_children)
    GetButtonByIndex(old_selected)->SetSelected(false);
  if (new_selected >= 0 && size_t{old_selected} < total_children)
    GetButtonByIndex(new_selected)->SetSelected(true);
}

bool PageIndicatorView::IsPageSelectedForTesting(int index) {
  return GetButtonByIndex(index)->selected();
}

}  // namespace ash