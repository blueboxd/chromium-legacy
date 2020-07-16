// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/base_control_test_widget.h"

#include <memory>
#include <utility>

namespace views {

namespace test {

BaseControlTestWidget::BaseControlTestWidget() = default;
BaseControlTestWidget::~BaseControlTestWidget() = default;

void BaseControlTestWidget::SetUp() {
  ViewsTestBase::SetUp();

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(200, 200);
  widget_->Init(std::move(params));
  auto* container = widget_->SetContentsView(std::make_unique<View>());

  CreateWidgetContent(container);

  widget_->Show();
}

void BaseControlTestWidget::TearDown() {
  widget_.reset();
  ViewsTestBase::TearDown();
}

void BaseControlTestWidget::CreateWidgetContent(View* container) {}

}  // namespace test
}  // namespace views
