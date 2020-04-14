// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

namespace {

class TestAXEventObserver : public AXEventObserver {
 public:
  TestAXEventObserver() { AXEventManager::Get()->AddObserver(this); }

  ~TestAXEventObserver() override {
    AXEventManager::Get()->RemoveObserver(this);
  }

  TestAXEventObserver(const TestAXEventObserver&) = delete;
  TestAXEventObserver& operator=(const TestAXEventObserver&) = delete;

  int value_changed_count() const { return value_changed_count_; }

  // AXEventObserver:
  void OnViewEvent(View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kValueChanged)
      value_changed_count_++;
  }

 private:
  int value_changed_count_ = 0;
};

}  // namespace

class ProgressBarTest : public ViewsTestBase {
 protected:
  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    bar_ = new ProgressBar;

    widget_ = CreateTestWidget();
    widget_->SetContentsView(bar_);
    widget_->Show();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ProgressBar* bar_;
  std::unique_ptr<Widget> widget_;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(ProgressBarTest, AccessibleNodeData) {
  bar_->SetValue(0.626);

  ui::AXNodeData node_data;
  bar_->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kProgressIndicator, node_data.role);
  EXPECT_EQ(base::string16(),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(std::string("62%"),
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kRestriction));
}

// Verifies the correct a11y events are raised for an accessible progress bar.
TEST_F(ProgressBarTest, AccessibilityEvents) {
  TestAXEventObserver observer;
  EXPECT_EQ(0, observer.value_changed_count());

  bar_->SetValue(0.50);
  EXPECT_EQ(1, observer.value_changed_count());

  bar_->SetValue(0.63);
  EXPECT_EQ(2, observer.value_changed_count());

  bar_->SetValue(0.636);
  EXPECT_EQ(2, observer.value_changed_count());

  bar_->SetValue(0.642);
  EXPECT_EQ(3, observer.value_changed_count());

  widget_->Hide();
  widget_->Show();
  EXPECT_EQ(3, observer.value_changed_count());

  widget_->Hide();
  bar_->SetValue(0.8);
  EXPECT_EQ(3, observer.value_changed_count());

  widget_->Show();
  EXPECT_EQ(4, observer.value_changed_count());
}

// Test that default colors can be overridden. Used by Chromecast.
TEST_F(ProgressBarTest, OverrideDefaultColors) {
  EXPECT_NE(SK_ColorRED, bar_->GetForegroundColor());
  EXPECT_NE(SK_ColorGREEN, bar_->GetBackgroundColor());
  EXPECT_NE(bar_->GetForegroundColor(), bar_->GetBackgroundColor());

  bar_->SetForegroundColor(SK_ColorRED);
  bar_->SetBackgroundColor(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorRED, bar_->GetForegroundColor());
  EXPECT_EQ(SK_ColorGREEN, bar_->GetBackgroundColor());
}

}  // namespace views
