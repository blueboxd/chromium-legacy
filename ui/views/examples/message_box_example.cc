// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/message_box_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

MessageBoxExample::MessageBoxExample()
    : ExampleBase(GetStringUTF8(IDS_MESSAGE_SELECT_LABEL).c_str()) {}

MessageBoxExample::~MessageBoxExample() = default;

void MessageBoxExample::CreateExampleView(View* container) {
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  auto message_box_view = std::make_unique<MessageBoxView>(
      MessageBoxView::InitParams(GetStringUTF16(IDS_MESSAGE_INTRO_LABEL)));
  message_box_view->SetCheckBoxLabel(
      GetStringUTF16(IDS_MESSAGE_CHECK_BOX_LABEL));

  const int message_box_column = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_box_column);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(1 /* expand */, message_box_column);
  message_box_view_ = layout->AddView(std::move(message_box_view));

  const int button_column = 1;
  column_set = layout->AddColumnSet(button_column);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0.5f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0.5f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(0 /* no expand */, button_column);

  status_ = layout->AddView(std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_MESSAGE_STATUS_LABEL)));
  toggle_ = layout->AddView(std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_MESSAGE_TOGGLE_LABEL)));
}

void MessageBoxExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == status_) {
    message_box_view_->SetCheckBoxLabel(
        message_box_view_->IsCheckBoxSelected()
            ? GetStringUTF16(IDS_MESSAGE_ON_LABEL)
            : GetStringUTF16(IDS_MESSAGE_OFF_LABEL));
    LogStatus(
        message_box_view_->IsCheckBoxSelected()
            ? GetStringUTF8(IDS_MESSAGE_CHECK_SELECTED_LABEL).c_str()
            : GetStringUTF8(IDS_MESSAGE_CHECK_NOT_SELECTED_LABEL).c_str());
  } else if (sender == toggle_) {
    message_box_view_->SetCheckBoxSelected(
        !message_box_view_->IsCheckBoxSelected());
  }
}

}  // namespace examples
}  // namespace views
