// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/tabbed_pane_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/grid_layout.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

TabbedPaneExample::TabbedPaneExample()
    : ExampleBase(GetStringUTF8(IDS_TABBED_PANE_SELECT_LABEL).c_str()) {}

TabbedPaneExample::~TabbedPaneExample() = default;

void TabbedPaneExample::CreateExampleView(View* container) {
  auto tabbed_pane = std::make_unique<TabbedPane>();
  tabbed_pane->set_listener(this);
  auto add = std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_TABBED_PANE_ADD_LABEL));
  auto add_at = std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_TABBED_PANE_ADD_1_LABEL));
  auto select_at = std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_TABBED_PANE_SELECT_1_LABEL));

  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  const int tabbed_pane_column = 0;
  ColumnSet* column_set = layout->AddColumnSet(tabbed_pane_column);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(1 /* expand */, tabbed_pane_column);
  tabbed_pane_ = layout->AddView(std::move(tabbed_pane));

  // Create a few tabs with a button first.
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_1_LABEL));
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_2_LABEL));
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_3_LABEL));

  // Add control buttons horizontally.
  const int button_column = 1;
  column_set = layout->AddColumnSet(button_column);
  for (size_t i = 0; i < 3; i++) {
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0f,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
  }

  layout->StartRow(0 /* no expand */, button_column);
  add_ = layout->AddView(std::move(add));
  add_at_ = layout->AddView(std::move(add_at));
  select_at_ = layout->AddView(std::move(select_at));
}

void TabbedPaneExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == add_) {
    AddButton(GetStringUTF16(IDS_TABBED_PANE_ADDED_LABEL));
  } else if (sender == add_at_) {
    const base::string16 label = GetStringUTF16(IDS_TABBED_PANE_ADDED_1_LABEL);
    tabbed_pane_->AddTabAtIndex(1, label,
                                std::make_unique<LabelButton>(nullptr, label));
  } else if (sender == select_at_) {
    if (tabbed_pane_->GetTabCount() > 1)
      tabbed_pane_->SelectTabAt(1);
  }
  PrintCurrentStatus();
}

void TabbedPaneExample::TabSelectedAt(int index) {
  // Just print the status when selection changes.
  PrintCurrentStatus();
}

void TabbedPaneExample::PrintCurrentStatus() {
  PrintStatus("Tab Count:%" PRIuS ", Selected Tab:%" PRIuS,
              tabbed_pane_->GetTabCount(), tabbed_pane_->GetSelectedTabIndex());
}

void TabbedPaneExample::AddButton(const base::string16& label) {
  tabbed_pane_->AddTab(label, std::make_unique<LabelButton>(nullptr, label));
}

}  // namespace examples
}  // namespace views
