// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/progress_bar_example.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

const double kStepSize = 0.1;

double SetToMax(double percent) {
  return std::min(std::max(percent, 0.0), 1.0);
}

}  // namespace

namespace views {
namespace examples {

ProgressBarExample::ProgressBarExample() : ExampleBase("Progress Bar") {}

ProgressBarExample::~ProgressBarExample() = default;

void ProgressBarExample::CreateExampleView(View* container) {
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::FIXED, 200, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  minus_button_ =
      layout->AddView(MdTextButton::Create(this, base::ASCIIToUTF16("-")));
  progress_bar_ = layout->AddView(std::make_unique<ProgressBar>());
  plus_button_ =
      layout->AddView(MdTextButton::Create(this, base::ASCIIToUTF16("+")));

  layout->StartRowWithPadding(0, 0, 0, 10);
  layout->AddView(
      std::make_unique<Label>(base::ASCIIToUTF16("Infinite loader:")));
  auto infinite_bar = std::make_unique<ProgressBar>();
  infinite_bar->SetValue(-1);
  layout->AddView(std::move(infinite_bar));

  layout->StartRowWithPadding(0, 0, 0, 10);
  layout->AddView(std::make_unique<Label>(
      base::ASCIIToUTF16("Infinite loader (very short):")));
  auto shorter_bar = std::make_unique<ProgressBar>(2);
  shorter_bar->SetValue(-1);
  layout->AddView(std::move(shorter_bar));
}

void ProgressBarExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == minus_button_)
    current_percent_ = SetToMax(current_percent_ - kStepSize);
  else if (sender == plus_button_)
    current_percent_ = SetToMax(current_percent_ + kStepSize);

  progress_bar_->SetValue(current_percent_);
}

}  // namespace examples
}  // namespace views
