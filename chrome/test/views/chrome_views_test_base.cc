// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_views_test_base.h"

#include "chrome/test/views/chrome_test_views_delegate.h"

ChromeViewsTestBase::~ChromeViewsTestBase() = default;

void ChromeViewsTestBase::SetUp() {
  set_views_delegate(std::make_unique<ChromeTestViewsDelegate>());
  views::ViewsTestBase::SetUp();
}

std::unique_ptr<views::Widget> ChromeViewsTestBase::CreateTestWidget(
    views::Widget::InitParams::Type type) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params = CreateParams(type);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 400, 400);
  widget->Init(std::move(params));
  return widget;
}
