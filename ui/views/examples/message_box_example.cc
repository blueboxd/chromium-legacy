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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

MessageBoxExample::MessageBoxExample()
    : ExampleBase(GetStringUTF8(IDS_MESSAGE_SELECT_LABEL).c_str()) {}

MessageBoxExample::~MessageBoxExample() = default;

void MessageBoxExample::CreateExampleView(View* container) {
  container->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  message_box_view_ = container->AddChildView(std::make_unique<MessageBoxView>(
      GetStringUTF16(IDS_MESSAGE_INTRO_LABEL)));
  message_box_view_->SetCheckBoxLabel(
      GetStringUTF16(IDS_MESSAGE_CHECK_BOX_LABEL));

  View* button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(LayoutAlignment::kStart);

  status_ = button_panel->AddChildView(std::make_unique<LabelButton>(
      this, GetStringUTF16(IDS_MESSAGE_STATUS_LABEL)));
  toggle_ = button_panel->AddChildView(std::make_unique<LabelButton>(
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
