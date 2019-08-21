// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout/animating_layout_manager.h"

#include <utility>
#include <vector>

#include "base/scoped_observer.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/layout/interpolating_layout_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// TODO(dfried): Remove these using statements when we migrate this class to
// ui/views.
using views::FlexLayout;
using views::FlexSpecification;
using views::kFlexBehaviorKey;
using views::kMarginsKey;
using views::LayoutAlignment;
using views::LayoutManager;
using views::LayoutManagerBase;
using views::LayoutOrientation;
using views::MaximumFlexSizeRule;
using views::MinimumFlexSizeRule;
using views::SizeBounds;
using views::View;
using views::ViewObserver;
using views::ViewsTestBase;
using views::Widget;

using ProposedLayout = LayoutManagerBase::ProposedLayout;

namespace {

class TestLayoutManager : public LayoutManagerBase {
 public:
  void SetLayout(const ProposedLayout& layout) {
    layout_ = layout;
    InvalidateLayout();
  }

  const ProposedLayout& layout() const { return layout_; }

 protected:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override {
    return layout_;
  }

 private:
  ProposedLayout layout_;
};

constexpr gfx::Size kChildViewSize{10, 10};

}  // anonymous namespace

// Test fixture which creates an AnimatingLayoutManager and instruments it so
// the animations can be directly controlled via gfx::AnimationContainerTestApi.
class AnimatingLayoutManagerSteppingTest : public testing::Test {
 public:
  void SetUp() override {
    // Don't use a unique_ptr because derived classes may want to own this view.
    view_ = new View();
    for (int i = 0; i < 3; ++i) {
      auto child = std::make_unique<View>();
      child->SetPreferredSize(kChildViewSize);
      children_.push_back(view_->AddChildView(std::move(child)));
    }

    animating_layout_manager_ =
        view_->SetLayoutManager(std::make_unique<AnimatingLayoutManager>());

    // Use linear transitions to make expected values predictable.
    animating_layout_manager_->SetTweenType(gfx::Tween::Type::LINEAR);
    animating_layout_manager_->SetAnimationDuration(
        base::TimeDelta::FromSeconds(1));

    if (UseContainerTestApi()) {
      container_test_api_ = std::make_unique<gfx::AnimationContainerTestApi>(
          animating_layout_manager_->GetAnimationContainerForTesting());
    }

    // These can't be constructed statically since they depend on the child
    // views.
    layout1_ = {{100, 100},
                {{children_[0], true, {5, 5, 10, 10}},
                 {children_[1], false},
                 {children_[2], true, {20, 20, 20, 20}}}};
    layout2_ = {{200, 200},
                {{children_[0], true, {10, 20, 20, 30}},
                 {children_[1], false},
                 {children_[2], true, {10, 100, 10, 10}}}};
  }

  void TearDown() override {
    delete view_;
    view_ = nullptr;
  }

  View* view() { return view_; }
  View* child(size_t index) const { return children_[index]; }
  size_t num_children() const { return children_.size(); }
  AnimatingLayoutManager* layout() { return animating_layout_manager_; }
  gfx::AnimationContainerTestApi* animation_api() {
    return container_test_api_.get();
  }
  const ProposedLayout& layout1() const { return layout1_; }
  const ProposedLayout& layout2() const { return layout2_; }

  void EnsureLayout(const ProposedLayout& expected) {
    for (size_t i = 0; i < expected.child_layouts.size(); ++i) {
      const auto& expected_child = expected.child_layouts[i];
      const View* const child = expected_child.child_view;
      EXPECT_EQ(view_, child->parent()) << " view " << i << " parent differs.";
      EXPECT_EQ(expected_child.visible, child->GetVisible())
          << " view " << i << " visibility.";
      if (expected_child.visible) {
        EXPECT_EQ(expected_child.bounds, child->bounds())
            << " view " << i << " bounds";
      }
    }
  }

  virtual bool UseContainerTestApi() const { return true; }

 private:
  ProposedLayout layout1_;
  ProposedLayout layout2_;
  View* view_;
  std::vector<View*> children_;
  AnimatingLayoutManager* animating_layout_manager_ = nullptr;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;
};

TEST_F(AnimatingLayoutManagerSteppingTest, SetLayoutManager_NoAnimation) {
  auto test_layout = std::make_unique<TestLayoutManager>();
  test_layout->SetLayout(layout1());
  layout()->SetShouldAnimateBounds(true);
  layout()->SetTargetLayoutManager(std::move(test_layout));

  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ResetLayout_NoAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();

  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest, HostInvalidate_TriggersAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();

  // At this point animation should have started, but not proceeded.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_AnimateBounds_AnimationProgresses) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->SetSize(view()->GetPreferredSize());
  ProposedLayout expected =
      InterpolatingLayoutManager::Interpolate(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->SetSize(view()->GetPreferredSize());
  expected = InterpolatingLayoutManager::Interpolate(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->SetSize(view()->GetPreferredSize());
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);
}

TEST_F(AnimatingLayoutManagerSteppingTest, TestEvents) {
  class EventWatcher : public AnimatingLayoutManager::Observer {
   public:
    ~EventWatcher() override {}

    explicit EventWatcher(AnimatingLayoutManager* layout) {
      scoped_observer_.Add(layout);
    }

    void OnLayoutIsAnimatingChanged(AnimatingLayoutManager* source,
                                    bool is_animating) override {
      events_.push_back(is_animating);
    }

    const std::vector<bool> events() const { return events_; }

   private:
    std::vector<bool> events_;
    ScopedObserver<AnimatingLayoutManager, Observer> scoped_observer_{this};
  };

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  EXPECT_FALSE(layout()->is_animating());
  EventWatcher watcher(layout());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  const std::vector<bool> expected1{true};
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, watcher.events());

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, watcher.events());

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  const std::vector<bool> expected2{true, false};
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected2, watcher.events());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_NoAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Because the desired layout did not change, there is no animation.
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostResize_NoAnimateBounds_NoAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Because the size of the host view changed, there is no animation.
  test_layout->SetLayout(layout2());
  view()->SetSize(view()->GetPreferredSize());
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout2().host_size, view()->size());
  EnsureLayout(layout2());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_NewLayoutTriggersAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_AnimationProgresses) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  view()->SetSize(view()->GetPreferredSize());

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  ProposedLayout expected =
      InterpolatingLayoutManager::Interpolate(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  expected = InterpolatingLayoutManager::Interpolate(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected);
}

// Test that when one view can flex to fill the space yielded by another view
// which is hidden, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_SlideAfterViewHidden) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = InterpolatingLayoutManager::Interpolate(
      0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = InterpolatingLayoutManager::Interpolate(0.5, expected_start,
                                                     expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when one view can flex to fill the space yielded by another view
// which is removed, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_SlideAfterViewRemoved) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  delete child(0);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = InterpolatingLayoutManager::Interpolate(
      0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = InterpolatingLayoutManager::Interpolate(0.5, expected_start,
                                                     expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when an animation starts and then the target changes mid-stream,
// the animation redirects.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_RedirectAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end1{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end2{
      {50, 20},
      {{child(0), false}, {child(1), true, {5, 5, 40, 10}}, {child(2), false}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = InterpolatingLayoutManager::Interpolate(
      0.5, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected =
      InterpolatingLayoutManager::Interpolate(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

// Test that when an animation starts and then the target changes near the end
// of the animation, the animation resets.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_ResetAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end1{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end2{
      {50, 20},
      {{child(0), false}, {child(1), true, {5, 5, 40, 10}}, {child(2), false}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(900));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = InterpolatingLayoutManager::Interpolate(
      0.9, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected =
      InterpolatingLayoutManager::Interpolate(0.0, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected =
      InterpolatingLayoutManager::Interpolate(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

namespace {

constexpr base::TimeDelta kMinimumAnimationTime =
    base::TimeDelta::FromMilliseconds(50);

// Layout manager which immediately lays out its child views when it is
// invalidated.
class ImmediateLayoutManager : public LayoutManager {
 public:
  explicit ImmediateLayoutManager(bool use_preferred_size)
      : use_preferred_size_(use_preferred_size) {}

  // LayoutManager:

  void InvalidateLayout() override { Layout(host_); }

  gfx::Size GetPreferredSize(const View* view) const override {
    return gfx::Size();
  }

  void Layout(View* view) override {
    EXPECT_EQ(host_, view);
    for (View* child : host_->children()) {
      if (use_preferred_size_) {
        const gfx::Size preferred = child->GetPreferredSize();
        if (preferred != child->size()) {
          // This implicityly lays out the child view.
          child->SetSize(preferred);
          continue;
        }
      }
      child->Layout();
    }
  }

  void Installed(View* host) override {
    DCHECK(!host_);
    host_ = host;
  }

 private:
  const bool use_preferred_size_;
  View* host_ = nullptr;
};

// Allows an AnimatingLayoutManager to be observed so that we can wait for an
// animation to complete in real time. Call WaitForAnimationToComplete() to
// pause execution until an animation (if any) is completed.
class AnimationWatcher : public AnimatingLayoutManager::Observer {
 public:
  explicit AnimationWatcher(AnimatingLayoutManager* layout_manager)
      : layout_manager_(layout_manager) {
    observer_.Add(layout_manager);
  }

  void OnLayoutIsAnimatingChanged(AnimatingLayoutManager*,
                                  bool is_animating) override {
    if (!is_animating && waiting_) {
      run_loop_->Quit();
      waiting_ = false;
    }
  }

  void WaitForAnimationToComplete() {
    if (!layout_manager_->is_animating())
      return;
    DCHECK(!waiting_);
    waiting_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  AnimatingLayoutManager* const layout_manager_;
  ScopedObserver<AnimatingLayoutManager, AnimatingLayoutManager::Observer>
      observer_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
  bool waiting_ = false;
};

}  // anonymous namespace

// Test fixture for testing animations in realtime. Provides a parent view with
// an ImmediateLayoutManager so that when animation frames are triggered, the
// host view is laid out immediately. Animation durations are kept short to
// prevent tests from taking too long.
class AnimatingLayoutManagerRealtimeTest
    : public AnimatingLayoutManagerSteppingTest {
 public:
  void SetUp() override {
    AnimatingLayoutManagerSteppingTest::SetUp();
    root_view_ = std::make_unique<View>();
    root_view_->AddChildView(view());
    animation_watcher_ = std::make_unique<AnimationWatcher>(layout());
  }

  void TearDown() override {
    animation_watcher_.reset();
    // Don't call base version because we own the view.
  }

  bool UseContainerTestApi() const override { return false; }

  void InitRootView() {
    root_view_->SetLayoutManager(std::make_unique<ImmediateLayoutManager>(
        layout()->should_animate_bounds()));
    layout()->EnableAnimationForTesting();
  }

  AnimationWatcher* animation_watcher() { return animation_watcher_.get(); }

 private:
  std::unique_ptr<View> root_view_;
  std::unique_ptr<AnimationWatcher> animation_watcher_;
};

TEST_F(AnimatingLayoutManagerRealtimeTest, TestAnimateSlide) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(true);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView();

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {35, 20},
      {{child(1), true, {{5, 5}, kChildViewSize}},
       {child(2), true, {{20, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected_start.host_size, view()->size());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  EXPECT_TRUE(layout()->is_animating());
  delete child(0);

  animation_watcher()->WaitForAnimationToComplete();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected_end.host_size, view()->size());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerRealtimeTest, TestAnimateStretch) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));
  InitRootView();

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(1), true, {{5, 5}, {25, kChildViewSize.height()}}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(view()->GetPreferredSize());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  EXPECT_TRUE(layout()->is_animating());
  delete child(0);
  animation_watcher()->WaitForAnimationToComplete();

  EnsureLayout(expected_end);
}

// TODO(dfried): figure out why these tests absolutely do not animate properly
// on Mac. Whatever magic makes the compositor animation runner go doesn't seem
// to want to work on Mac in non-browsertests :(
#if !defined(OS_MACOSX)

// Test fixture for testing sequences of the following four actions:
// * animating layout manager configured on host view
// * host view added to parent view
// * parent view added to widget
// * child view added to host view
//
// The result will either be an animation or no animation, but both will have
// the same final layout. We will not test all possible sequences, but a
// representative sample based on what sequences of actions we are (a) likely to
// see and (b) hit most possible code paths.
class AnimatingLayoutManagerSequenceTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_.reset(new Widget());
    auto params = CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = {0, 0, 500, 500};
    widget_->Init(std::move(params));

    parent_view_ptr_ = std::make_unique<View>();
    parent_view_ptr_->SetLayoutManager(
        std::make_unique<ImmediateLayoutManager>(true));
    parent_view_ = parent_view_ptr_.get();

    layout_view_ptr_ = std::make_unique<View>();
    layout_view_ = layout_view_ptr_.get();
  }

  void TearDown() override {
    // Do before rest of tear down.
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void ConfigureLayoutView() {
    layout_manager_ = layout_view_->SetLayoutManager(
        std::make_unique<AnimatingLayoutManager>());
    layout_manager_->SetTweenType(gfx::Tween::Type::LINEAR);
    layout_manager_->SetAnimationDuration(kMinimumAnimationTime);
    auto* const flex_layout =
        layout_manager_->SetTargetLayoutManager(std::make_unique<FlexLayout>());
    flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
    flex_layout->SetCollapseMargins(true);
    flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
    flex_layout->SetDefault(kMarginsKey, gfx::Insets(5));
    layout_manager_->SetShouldAnimateBounds(true);
  }

  void AddViewToParent() {
    parent_view_->AddChildView(std::move(layout_view_ptr_));
  }

  void AddParentToWidget() {
    widget_->GetRootView()->AddChildView(std::move(parent_view_ptr_));
  }

  void AddChildToLayoutView() {
    auto child_view_ptr = std::make_unique<View>();
    child_view_ptr->SetPreferredSize(gfx::Size(10, 10));
    child_view_ = layout_view_->AddChildView(std::move(child_view_ptr));
  }

  void ExpectResetToLayout() {
    EXPECT_FALSE(layout_manager_->is_animating());
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

  void ExpectAnimateToLayout() {
    EXPECT_TRUE(layout_manager_->is_animating());
    AnimationWatcher animation_watcher(layout_manager_);
    animation_watcher.WaitForAnimationToComplete();
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

 private:
  struct WidgetCloser {
    inline void operator()(Widget* widget) const { widget->CloseNow(); }
  };

  using WidgetAutoclosePtr = std::unique_ptr<Widget, WidgetCloser>;

  AnimatingLayoutManager* layout_manager_ = nullptr;
  View* child_view_ = nullptr;
  View* parent_view_ = nullptr;
  View* layout_view_ = nullptr;
  std::unique_ptr<View> parent_view_ptr_;
  std::unique_ptr<View> layout_view_ptr_;
  WidgetAutoclosePtr widget_;
};

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddChild_AddToParent_Configure_AddToWidget) {
  AddChildToLayoutView();
  AddViewToParent();
  ConfigureLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddChild_Configure_AddToParent_AddToWidget) {
  AddChildToLayoutView();
  ConfigureLayoutView();
  AddViewToParent();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddChild_AddToParent_AddToWidget) {
  ConfigureLayoutView();
  AddChildToLayoutView();
  AddViewToParent();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddToParent_AddChild_AddToWidget) {
  ConfigureLayoutView();
  AddViewToParent();
  AddChildToLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_Configure_AddChild_AddToWidget) {
  AddViewToParent();
  ConfigureLayoutView();
  AddChildToLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_AddChild_Configure_AddToWidget) {
  AddViewToParent();
  AddChildToLayoutView();
  ConfigureLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToWidget_AddToParent_Configure_AddChild) {
  AddParentToWidget();
  AddViewToParent();
  ConfigureLayoutView();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_AddToWidget_Configure_AddChild) {
  AddViewToParent();
  AddParentToWidget();
  ConfigureLayoutView();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddToParent_AddToWidget_AddChild) {
  ConfigureLayoutView();
  AddViewToParent();
  AddParentToWidget();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToWidget_AddToParent_AddChild_Configure) {
  AddParentToWidget();
  AddViewToParent();
  AddChildToLayoutView();
  ConfigureLayoutView();

  ExpectResetToLayout();
}

#endif  // !defined(OS_MACOSX)
