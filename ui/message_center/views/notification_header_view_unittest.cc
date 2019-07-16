// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

class NotificationHeaderViewTest : public views::ViewsTestBase {
 public:
  NotificationHeaderViewTest() = default;
  ~NotificationHeaderViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    // Setup a mocked time environment.
    scoped_task_environment_ = new ScopedTaskEnvironment(
        ScopedTaskEnvironment::MainThreadType::UI,
        ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);
    set_scoped_task_environment(base::WrapUnique(scoped_task_environment_));

    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
    views::View* container = new views::View();
    widget_.SetContentsView(container);

    notification_header_view_ = new NotificationHeaderView(nullptr);
    container->AddChildView(notification_header_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  NotificationHeaderView* notification_header_view_ = nullptr;
  ScopedTaskEnvironment* scoped_task_environment_ = nullptr;

 private:
  views::Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(NotificationHeaderViewTest);
};

TEST_F(NotificationHeaderViewTest, UpdatesTimestampOverTime) {
  notification_header_view_->SetTimestamp(base::Time::Now() +
                                          base::TimeDelta::FromHours(3) +
                                          base::TimeDelta::FromMinutes(30));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 3),
            notification_header_view_->timestamp_for_testing());

  scoped_task_environment_->FastForwardBy(base::TimeDelta::FromHours(3));
  scoped_task_environment_->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 30),
            notification_header_view_->timestamp_for_testing());

  scoped_task_environment_->FastForwardBy(base::TimeDelta::FromMinutes(30));
  scoped_task_environment_->RunUntilIdle();

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      notification_header_view_->timestamp_for_testing());

  scoped_task_environment_->FastForwardBy(base::TimeDelta::FromDays(2));
  scoped_task_environment_->RunUntilIdle();

  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 2),
            notification_header_view_->timestamp_for_testing());
}

}  // namespace message_center
