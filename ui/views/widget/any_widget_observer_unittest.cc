// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer.h"

#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_test.h"

namespace {

using views::Widget;

using AnyWidgetObserverTest = views::test::WidgetTest;

TEST_F(AnyWidgetObserverTest, ObservesInitialize) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool initialized = false;

  observer.set_initialized_callback(
      base::BindLambdaForTesting([&](Widget*) { initialized = true; }));

  EXPECT_FALSE(initialized);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  EXPECT_TRUE(initialized);
}

TEST_F(AnyWidgetObserverTest, ObservesClose) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool closing = false;

  observer.set_closing_callback(
      base::BindLambdaForTesting([&](Widget*) { closing = true; }));

  EXPECT_FALSE(closing);
  { WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget()); }
  EXPECT_TRUE(closing);
}

TEST_F(AnyWidgetObserverTest, ObservesShow) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool shown = false;

  observer.set_shown_callback(
      base::BindLambdaForTesting([&](Widget*) { shown = true; }));

  EXPECT_FALSE(shown);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  w0->Show();
  EXPECT_TRUE(shown);
}

TEST_F(AnyWidgetObserverTest, ObservesHide) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool hidden = false;

  observer.set_hidden_callback(
      base::BindLambdaForTesting([&](Widget*) { hidden = true; }));

  EXPECT_FALSE(hidden);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  w0->Hide();
  EXPECT_TRUE(hidden);
}

}  // namespace
