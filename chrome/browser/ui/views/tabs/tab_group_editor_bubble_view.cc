// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

// Returns our hard-coded set of colors.
const std::vector<std::pair<SkColor, base::string16>>& GetColorPickerList() {
  static const base::NoDestructor<
      std::vector<std::pair<SkColor, base::string16>>>
      list({{gfx::kGoogleBlue600, base::ASCIIToUTF16("Blue")},
            {gfx::kGoogleRed600, base::ASCIIToUTF16("Red")},
            {gfx::kGoogleYellow600, base::ASCIIToUTF16("Yellow")},
            {gfx::kGoogleGreen600, base::ASCIIToUTF16("Green")},
            {gfx::kGoogleOrange600, base::ASCIIToUTF16("Orange")},
            {gfx::kGooglePink600, base::ASCIIToUTF16("Pink")},
            {gfx::kGooglePurple600, base::ASCIIToUTF16("Purple")},
            {gfx::kGoogleCyan600, base::ASCIIToUTF16("Cyan")}});
  return *list;
}

}  // namespace

// static
void TabGroupEditorBubbleView::Show(views::View* anchor_view,
                                    TabController* tab_controller,
                                    TabGroupId group) {
  BubbleDialogDelegateView::CreateBubble(
      new TabGroupEditorBubbleView(anchor_view, tab_controller, group))
      ->Show();
}

gfx::Size TabGroupEditorBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

base::string16 TabGroupEditorBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/989174): remove this and other hardcoded ASCII strings.
  return base::ASCIIToUTF16("Customize tab group visuals");
}

bool TabGroupEditorBubbleView::Accept() {
  TabGroupVisualData old_data = *tab_controller_->GetVisualDataForGroup(group_);

  base::Optional<SkColor> selected_color = color_selector_->GetSelectedColor();
  const SkColor color =
      selected_color.has_value() ? selected_color.value() : old_data.color();
  TabGroupVisualData new_data(title_field_->GetText(), color);

  tab_controller_->SetVisualDataForGroup(group_, new_data);

  return true;
}

TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    views::View* anchor_view,
    TabController* tab_controller,
    TabGroupId group)
    : tab_controller_(tab_controller), group_(group) {
  SetAnchorView(anchor_view);

  const auto* layout_provider = ChromeLayoutProvider::Get();
  const TabGroupVisualData* current_data =
      tab_controller_->GetVisualDataForGroup(group_);

  // Add the text field for editing the title along with a label above it.
  View* title_label = AddChildView(std::make_unique<views::Label>(
      base::ASCIIToUTF16("New title"), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_field_ = AddChildView(std::make_unique<views::Textfield>());
  title_field_->SetAssociatedLabel(title_label);
  title_field_->SetDefaultWidthInChars(15);
  title_field_->SetText(current_data->title());

  title_field_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, 0,
                  layout_provider->GetDistanceMetric(
                      views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                  0));

  color_selector_ =
      AddChildView(std::make_unique<ColorPickerView>(GetColorPickerList()));

  // Layout vertically with margin collapsing. This allows us to use spacer
  // views with |DISTANCE_UNRELATED_CONTROL_VERTICAL| margins without worrying
  // about the default |DISTANCE_RELATED_CONTROL_VERTICAL| spacing.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL),
      true /* collapse_margins_spacing */);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout));
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;
