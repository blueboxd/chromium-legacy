// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/color_picker_view.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

// Represents one of the colors the user can pick from. Displayed as a solid
// circle of the given color.
class ColorPickerElementView : public views::Button,
                               public views::ButtonListener {
 public:
  ColorPickerElementView(
      base::RepeatingCallback<void(ColorPickerElementView*)> selected_callback,
      SkColor color,
      base::string16 color_name)
      : Button(this),
        selected_callback_(std::move(selected_callback)),
        color_(color),
        color_name_(color_name) {
    DCHECK(selected_callback_);

    SetAccessibleName(color_name);
    SetFocusForPlatform();
    SetInstallFocusRingOnFocus(true);

    SetBorder(
        views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));

    SetInkDropMode(InkDropMode::OFF);
    set_animate_on_state_change(true);
  }

  SkColor color() const { return color_; }

  void SetSelected(bool selected) {
    if (selected_ == selected)
      return;
    selected_ = selected;
    SchedulePaint();
  }

  bool selected() const { return selected_; }

  // views::Button:
  bool IsGroupFocusTraversable() const override {
    // Tab should only focus the selected element.
    return false;
  }

  views::View* GetSelectedViewForGroup(int group) override {
    DCHECK(parent());
    return parent()->GetSelectedViewForGroup(group);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::Button::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kRadioButton;
    node_data->SetCheckedState(selected() ? ax::mojom::CheckedState::kTrue
                                          : ax::mojom::CheckedState::kFalse);
  }

  base::string16 GetTooltipText(const gfx::Point& p) const override {
    return color_name_;
  }

  gfx::Size CalculatePreferredSize() const override {
    const gfx::Insets insets = GetInsets();
    gfx::Size size(24, 24);
    size.Enlarge(insets.width(), insets.height());
    return size;
  }

  int GetHeightForWidth(int width) const override { return width; }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    if (size() == previous_bounds.size())
      return;

    // Our highlight path should be slightly larger than the circle we paint.
    gfx::RectF bounds(GetContentsBounds());
    bounds.Inset(gfx::Insets(-2.0f));
    const gfx::PointF center = bounds.CenterPoint();
    SkPath path;
    path.addCircle(center.x(), center.y(), bounds.width() / 2.0f);
    SetProperty(views::kHighlightPathKey, std::move(path));
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    // Paint a colored circle surrounded by a bit of empty space.

    gfx::RectF bounds(GetContentsBounds());
    // We should be a circle.
    DCHECK_EQ(bounds.width(), bounds.height());

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color_);
    flags.setAntiAlias(true);
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2.0f, flags);

    PaintSelectionIndicator(canvas);
  }

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK_EQ(this, sender);

    selected_ = !selected_;
    SchedulePaint();
    selected_callback_.Run(this);
  }

 private:
  // Paints a ring in our color circle to indicate selection or mouse hover.
  // Does nothing if not selected or hovered.
  void PaintSelectionIndicator(gfx::Canvas* canvas) {
    // Visual parameters of our ring.
    constexpr float kInset = 4.0f;
    constexpr float kThickness = 4.0f;
    constexpr SkColor kSelectedColor = SK_ColorWHITE;
    constexpr SkColor kPendingColor = gfx::kGoogleGrey200;

    SkColor paint_color = gfx::kPlaceholderColor;
    if (selected_) {
      paint_color = kSelectedColor;
    } else if (GetVisualState() == STATE_HOVERED ||
               hover_animation().is_animating()) {
      const float alpha = gfx::Tween::CalculateValue(
          gfx::Tween::FAST_OUT_SLOW_IN, hover_animation().GetCurrentValue());
      paint_color = color_utils::AlphaBlend(kPendingColor, color_, alpha);
    } else {
      return;
    }

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kThickness);
    flags.setAntiAlias(true);
    flags.setColor(paint_color);

    gfx::RectF indicator_bounds(GetContentsBounds());
    indicator_bounds.Inset(gfx::InsetsF(kInset));
    DCHECK(!indicator_bounds.size().IsEmpty());
    canvas->DrawCircle(indicator_bounds.CenterPoint(),
                       indicator_bounds.width() / 2.0f, flags);
  }

  base::RepeatingCallback<void(ColorPickerElementView*)> selected_callback_;
  SkColor color_;
  base::string16 color_name_;
  bool selected_ = false;
};

ColorPickerView::ColorPickerView(
    base::span<const std::pair<SkColor, base::string16>> colors) {
  elements_.reserve(colors.size());
  for (const auto& color : colors) {
    // Create the views for each color, passing them our callback and saving
    // references to them. base::Unretained() is safe here since we delete these
    // views in our destructor, ensuring we outlive them.
    elements_.push_back(AddChildView(std::make_unique<ColorPickerElementView>(
        base::Bind(&ColorPickerView::OnColorSelected, base::Unretained(this)),
        color.first, color.second)));
  }

  // Our children should take keyboard focus, not us.
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
  for (View* view : elements_) {
    // Group the colors so they can be navigated with arrows.
    view->SetGroup(0);
  }

  const int element_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  views::DISTANCE_RELATED_BUTTON_HORIZONTAL) /
                              2;

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      element_spacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

ColorPickerView::~ColorPickerView() {
  // Remove child views early since they have references to us through a
  // callback.
  RemoveAllChildViews(true);
}

base::Optional<SkColor> ColorPickerView::GetSelectedColor() const {
  for (const ColorPickerElementView* element : elements_) {
    if (element->selected())
      return element->color();
  }
  return base::nullopt;
}

views::View* ColorPickerView::GetSelectedViewForGroup(int group) {
  for (ColorPickerElementView* element : elements_) {
    if (element->selected())
      return element;
  }
  return nullptr;
}

views::Button* ColorPickerView::GetElementAtIndexForTesting(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(elements_.size()));
  return elements_[index];
}

void ColorPickerView::OnColorSelected(ColorPickerElementView* element) {
  // Unselect all other elements so that only one can be selected at a time.
  for (ColorPickerElementView* other_element : elements_) {
    if (other_element != element)
      other_element->SetSelected(false);
  }
}
