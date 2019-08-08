// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {

// Returns our hard-coded set of colors.
const base::flat_map<std::string, SkColor>& GetColorPickerMap() {
  static const base::NoDestructor<base::flat_map<std::string, SkColor>> map(
      {{"Blue", gfx::kGoogleBlue600},
       {"Red", gfx::kGoogleRed600},
       {"Yellow", gfx::kGoogleYellow600},
       {"Green", gfx::kGoogleGreen600},
       {"Orange", gfx::kGoogleOrange600},
       {"Pink", gfx::kGooglePink600},
       {"Purple", gfx::kGooglePurple600},
       {"Cyan", gfx::kGoogleCyan600}});
  return *map;
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

  base::string16 title = title_field_->GetText();
  if (title.empty())
    title = old_data.title();

  const base::string16 color_name =
      color_selector_->model()->GetItemAt(color_selector_->GetSelectedIndex());
  const SkColor color =
      GetColorPickerMap().find(base::UTF16ToASCII(color_name))->second;
  TabGroupVisualData new_data(std::move(title), color);

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

  // Add the text field for editing the title along with a label above it.
  AddChildView(std::make_unique<views::Label>(base::ASCIIToUTF16("New title:"),
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_PRIMARY));
  title_field_ = AddChildView(std::make_unique<views::Textfield>());
  title_field_->SetDefaultWidthInChars(15);

  title_field_->SetProperty(
      views::kMarginsKey,
      gfx::Insets(0, 0,
                  layout_provider->GetDistanceMetric(
                      views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                  0));

  std::vector<base::string16> color_names;
  for (const auto& entry : GetColorPickerMap())
    color_names.push_back(base::ASCIIToUTF16(entry.first));
  auto combobox_model = std::make_unique<ui::SimpleComboboxModel>(color_names);

  // Add the color selector with label above it.
  AddChildView(std::make_unique<views::Label>(base::ASCIIToUTF16("New color:"),
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_PRIMARY));
  color_selector_ = AddChildView(
      std::make_unique<views::Combobox>(std::move(combobox_model)));

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
