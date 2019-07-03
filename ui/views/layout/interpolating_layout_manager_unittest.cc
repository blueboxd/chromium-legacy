// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/interpolating_layout_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

namespace {

class TestLayout : public LayoutManagerBase {
 public:
  int num_layouts_generated() const { return num_layouts_generated_; }

  ProposedLayout GetProposedLayout(
      const SizeBounds& size_bounds) const override {
    ++num_layouts_generated_;
    ProposedLayout layout;
    layout.host_size = gfx::Size(10, 11);
    for (auto it = host_view()->children().begin();
         it != host_view()->children().end(); ++it) {
      if (!IsChildIncludedInLayout(*it))
        continue;
      ChildLayout child_layout;
      child_layout.child_view = *it;
      child_layout.visible = true;
      child_layout.bounds = gfx::Rect(1, 1, 1, 1);
      layout.child_layouts.push_back(child_layout);
    }
    return layout;
  }

 private:
  mutable int num_layouts_generated_ = 0;
};

}  // anonymous namespace

class InterpolatingLayoutManagerTest : public testing::Test {
 public:
  void SetUp() override {
    host_view_ = std::make_unique<View>();
    layout_manager_ = host_view_->SetLayoutManager(
        std::make_unique<InterpolatingLayoutManager>());
  }

  InterpolatingLayoutManager* layout_manager() { return layout_manager_; }
  View* host_view() { return host_view_.get(); }

 private:
  InterpolatingLayoutManager* layout_manager_ = nullptr;
  std::unique_ptr<View> host_view_;
};

TEST_F(InterpolatingLayoutManagerTest, SetDefaultLayout) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds());
  EXPECT_EQ(1, default_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, AddLayout_CheckZeroAndUnbounded) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {5, 0});
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(0, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds());
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(0, 0));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_HardBoundary) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {5, 0});
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(0, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(5, 2));
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(4, 2));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_SoftBoudnary) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(0, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(5, 2));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(4, 2));
  EXPECT_EQ(2, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(6, 6));
  EXPECT_EQ(2, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, GetProposedLayout_MultipleLayouts) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout1 =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  TestLayout* const other_layout2 =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {6, 2});
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(0, other_layout1->num_layouts_generated());
  EXPECT_EQ(0, other_layout2->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(5, 2));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout1->num_layouts_generated());
  EXPECT_EQ(0, other_layout2->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(6, 3));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout1->num_layouts_generated());
  EXPECT_EQ(0, other_layout2->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(7, 6));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(3, other_layout1->num_layouts_generated());
  EXPECT_EQ(1, other_layout2->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(20, 3));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(3, other_layout1->num_layouts_generated());
  EXPECT_EQ(2, other_layout2->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, InvalidateLayout) {
  static const gfx::Size kLayoutSize(5, 5);

  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  host_view()->SetSize(kLayoutSize);
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  host_view()->Layout();
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->InvalidateLayout();
  host_view()->Layout();
  EXPECT_EQ(2, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout->num_layouts_generated());
  host_view()->Layout();
  EXPECT_EQ(2, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout->num_layouts_generated());
}

TEST_F(InterpolatingLayoutManagerTest, SetOrientation) {
  TestLayout* const default_layout =
      layout_manager()->SetDefaultLayout(std::make_unique<TestLayout>());
  TestLayout* const other_layout =
      layout_manager()->AddLayout(std::make_unique<TestLayout>(), {4, 2});
  layout_manager()->SetOrientation(LayoutOrientation::kVertical);
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(0, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(2, 6));
  EXPECT_EQ(0, default_layout->num_layouts_generated());
  EXPECT_EQ(1, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(3, 5));
  EXPECT_EQ(1, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout->num_layouts_generated());
  layout_manager()->GetProposedLayout(SizeBounds(10, 3));
  EXPECT_EQ(2, default_layout->num_layouts_generated());
  EXPECT_EQ(2, other_layout->num_layouts_generated());
}

}  // namespace views
