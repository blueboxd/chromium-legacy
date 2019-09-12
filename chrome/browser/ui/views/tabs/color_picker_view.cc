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
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"

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

    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetAccessibleName(color_name);

    SetBorder(
        views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));

    set_has_ink_drop_action_on_click(false);
    set_ink_drop_base_color(SK_ColorBLACK);
    SetInkDropMode(InkDropMode::ON);
  }

  SkColor color() const { return color_; }

  void SetSelected(bool selected) {
    if (selected_ == selected)
      return;
    selected_ = selected;
    UpdateVisualsForSelection(nullptr);
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
    gfx::Size size(16, 16);
    size.Enlarge(insets.width(), insets.height());
    return size;
  }

  int GetHeightForWidth(int width) const override { return width; }

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
  }

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK_EQ(this, sender);

    selected_ = !selected_;
    UpdateVisualsForSelection(&event);
    selected_callback_.Run(this);
  }

 private:
  void UpdateVisualsForSelection(const ui::Event* selection_event) {
    // TODO(crbug.com/989174): display selection a better way; the ink drop is
    // ugly.
    AnimateInkDrop(
        selected_ ? views::InkDropState::ACTIVATED
                  : views::InkDropState::DEACTIVATED,
        selection_event != nullptr && selection_event->IsLocatedEvent()
            ? selection_event->AsLocatedEvent()
            : nullptr);
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
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

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
