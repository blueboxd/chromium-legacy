// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/result_selection_controller.h"

#include <gtest/gtest.h>
#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "base/macros.h"
#include "ui/events/event.h"

namespace app_list {
namespace {

class TestResultView : public SearchResultBaseView {
 public:
  TestResultView() = default;
  ~TestResultView() override = default;

  void ButtonPressed(Button* sender, const ui::Event& event) override {
    // Do nothing for test.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResultView);
};

// Allows immediate invocation of |VerticalTestContainer| and its derivatives,
// by handling the fake delegate's setup.
class TestContainerDelegateHarness {
 public:
  TestContainerDelegateHarness() {
    app_list_test_delegate_ =
        std::make_unique<app_list::test::AppListTestViewDelegate>();
  }

  ~TestContainerDelegateHarness() = default;

 protected:
  std::unique_ptr<app_list::test::AppListTestViewDelegate>
      app_list_test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestContainerDelegateHarness);
};

class VerticalTestContainer : public TestContainerDelegateHarness,
                              public SearchResultContainerView {
 public:
  explicit VerticalTestContainer(int num_results)
      : SearchResultContainerView(app_list_test_delegate_.get()) {
    for (int i = 0; i < num_results; ++i) {
      search_result_views_.emplace_back(std::make_unique<TestResultView>());
      search_result_views_.back()->set_index_in_container(i);
    }
    Update();
  }
  ~VerticalTestContainer() override = default;

  TestResultView* GetResultViewAt(size_t index) override {
    DCHECK_LT(index, search_result_views_.size());
    return search_result_views_[index].get();
  }

 private:
  int DoUpdate() override { return search_result_views_.size(); }

  std::vector<std::unique_ptr<TestResultView>> search_result_views_;

  DISALLOW_COPY_AND_ASSIGN(VerticalTestContainer);
};

class HorizontalTestContainer : public VerticalTestContainer {
 public:
  explicit HorizontalTestContainer(int num_results)
      : VerticalTestContainer(num_results) {
    set_horizontally_traversable(true);
  }

  ~HorizontalTestContainer() override = default;

  DISALLOW_COPY_AND_ASSIGN(HorizontalTestContainer);
};

class ResultSelectionTest : public testing::Test,
                            public testing::WithParamInterface<bool> {
 public:
  ResultSelectionTest() = default;
  ~ResultSelectionTest() override = default;

  void SetUp() override {
    if (testing::UnitTest::GetInstance()->current_test_info()->value_param()) {
      // Setup right to left environment if necessary.
      is_rtl_ = GetParam();
      if (is_rtl_)
        base::i18n::SetICUDefaultLocale("he");
    }

    if (!is_rtl_) {
      // Reset RTL if not needed.
      base::i18n::SetICUDefaultLocale("en");
    }

    result_selection_controller_ =
        std::make_unique<ResultSelectionController>(&containers_);

    testing::Test::SetUp();
  }

 protected:
  std::vector<std::unique_ptr<SearchResultContainerView>>
  CreateContainerVector(int container_count, int results, bool horizontal) {
    std::vector<std::unique_ptr<SearchResultContainerView>> containers;
    for (int i = 0; i < container_count; i++) {
      if (horizontal) {
        containers.emplace_back(
            std::make_unique<HorizontalTestContainer>(results));
      } else {
        containers.emplace_back(
            std::make_unique<VerticalTestContainer>(results));
      }
    }
    return containers;
  }

  std::vector<ResultLocationDetails> CreateLocationVector(
      int results,
      bool horizontal,
      int container_count = 1,
      int container_index = 0) {
    std::vector<ResultLocationDetails> locations;
    for (int i = 0; i < results; i++) {
      locations.emplace_back(ResultLocationDetails(
          container_index /*container_index*/,
          container_count /*container_count*/, i /*result_index*/,
          results /*result_count*/, horizontal /*container_is_horizontal*/));
    }
    return locations;
  }

  void SetContainers(
      const std::vector<std::unique_ptr<SearchResultContainerView>>&
          containers) {
    containers_.clear();
    for (auto& container : containers) {
      containers_.emplace_back(container.get());
    }
  }

  void TestSingleAxisTraversal(ui::KeyEvent* forward, ui::KeyEvent* backward) {
    const bool horizontal =
        result_selection_controller_->selected_location_details()
            ->container_is_horizontal;

    std::vector<ResultLocationDetails> locations =
        CreateLocationVector(4, horizontal);

    // These are divided to give as much detail as possible for a test failure.
    TestSingleAxisForward(forward, locations);
    TestSingleAxisLoop(forward, locations);
    TestSingleAxisLoopBack(backward, locations);
    TestSingleAxisBackward(backward, locations);
  }

  void TestSingleAxisForward(
      ui::KeyEvent* forward,
      const std::vector<ResultLocationDetails>& locations) {
    // Starts at the beginning
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[0]);
    for (int i = 1; i < (int)locations.size(); i++) {
      result_selection_controller_->MoveSelection(*forward);
      ASSERT_EQ(*result_selection_controller_->selected_location_details(),
                locations[i]);
    }
    // Expect steady traversal to the end
  }

  void TestSingleAxisLoop(ui::KeyEvent* forward,
                          const std::vector<ResultLocationDetails>& locations) {
    // Starts at the end
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[3]);

    // Expect loop back to first result.
    result_selection_controller_->MoveSelection(*forward);
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[0]);
  }

  void TestSingleAxisLoopBack(
      ui::KeyEvent* backward,
      const std::vector<ResultLocationDetails>& locations) {
    // Starts at the first
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[0]);

    // Expect loop back to last result.
    result_selection_controller_->MoveSelection(*backward);
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[3]);
  }

  void TestSingleAxisBackward(
      ui::KeyEvent* backward,
      const std::vector<ResultLocationDetails>& locations) {
    const int last_index = (int)locations.size() - 1;

    // Test reverse direction from last result
    ASSERT_EQ(*result_selection_controller_->selected_location_details(),
              locations[last_index]);
    for (int i = last_index - 1; i >= 0; i--) {
      result_selection_controller_->MoveSelection(*backward);
      ASSERT_EQ(*result_selection_controller_->selected_location_details(),
                locations[i]);
    }
  }

  void TestMultiAxisTraversal(bool tab) {
    ui::KeyEvent* forward;
    ui::KeyEvent* backward;
    ui::KeyEvent* vertical_forward;
    ui::KeyEvent* vertical_backward;
    if (tab) {
      // Set up for tab traversal
      forward = vertical_forward = &tab_key_;
      backward = vertical_backward = &shift_tab_key_;
    } else {
      // Set up for arrow key traversal
      forward = is_rtl_ ? &left_arrow_ : &right_arrow_;
      backward = is_rtl_ ? &right_arrow_ : &left_arrow_;
      vertical_forward = &down_arrow_;
      vertical_backward = &up_arrow_;
    }

    int num_containers = containers_.size();

    for (int i = 0; i < num_containers; i++) {
      const bool horizontal =
          result_selection_controller_->selected_location_details()
              ->container_is_horizontal;

      std::vector<ResultLocationDetails> locations =
          CreateLocationVector(4, horizontal, num_containers, i);

      if (horizontal && !tab) {
        TestSingleAxisForward(forward, locations);
        TestSingleAxisLoop(forward, locations);
        TestSingleAxisLoopBack(backward, locations);
        TestSingleAxisBackward(backward, locations);
        TestSingleAxisLoopBack(backward, locations);
        // End on the last result.
      } else {
        // Tab and Vertical traversal are identical
        TestSingleAxisForward(vertical_forward, locations);
        TestSingleAxisBackward(vertical_backward, locations);
        TestSingleAxisForward(vertical_forward, locations);
        // End on the last result.
      }

      // Change Containers
      result_selection_controller_->MoveSelection(*vertical_forward);
    }
  }

  std::unique_ptr<ResultSelectionController> result_selection_controller_;
  std::vector<SearchResultContainerView*> containers_;

  // Set up key events for test. These will never be marked as 'handled'.
  ui::KeyEvent down_arrow_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_DOWN, ui::EF_NONE);
  ui::KeyEvent up_arrow_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::EF_NONE);
  ui::KeyEvent left_arrow_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, ui::EF_NONE);
  ui::KeyEvent right_arrow_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, ui::EF_NONE);
  ui::KeyEvent tab_key_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
  ui::KeyEvent shift_tab_key_ =
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_SHIFT_DOWN);

  bool is_rtl_ = false;

  DISALLOW_COPY_AND_ASSIGN(ResultSelectionTest);
};

INSTANTIATE_TEST_SUITE_P(RTL, ResultSelectionTest, testing::Bool());

}  // namespace

TEST_F(ResultSelectionTest, VerticalTraversalOneContainerArrowKeys) {
  std::unique_ptr<VerticalTestContainer> vertical_container =
      std::make_unique<VerticalTestContainer>(4);
  // The vertical container is not horizontally traversable
  ASSERT_FALSE(vertical_container->horizontally_traversable());

  containers_.clear();
  containers_.emplace_back(vertical_container.get());

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestSingleAxisTraversal(&down_arrow_, &up_arrow_);
}

TEST_F(ResultSelectionTest, VerticalTraversalOneContainerTabKey) {
  std::unique_ptr<VerticalTestContainer> vertical_container =
      std::make_unique<VerticalTestContainer>(4);

  // The vertical container is not horizontally traversable
  ASSERT_FALSE(vertical_container->horizontally_traversable());

  containers_.clear();
  containers_.emplace_back(vertical_container.get());

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestSingleAxisTraversal(&tab_key_, &shift_tab_key_);
}

TEST_P(ResultSelectionTest, HorizontalTraversalOneContainerArrowKeys) {
  ui::KeyEvent* forward = is_rtl_ ? &left_arrow_ : &right_arrow_;
  ui::KeyEvent* backward = is_rtl_ ? &right_arrow_ : &left_arrow_;

  std::unique_ptr<HorizontalTestContainer> horizontal_container =
      std::make_unique<HorizontalTestContainer>(4);

  // The horizontal container is horizontally traversable
  ASSERT_TRUE(horizontal_container->horizontally_traversable());

  containers_.clear();
  containers_.emplace_back(horizontal_container.get());

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestSingleAxisTraversal(forward, backward);
}

TEST_P(ResultSelectionTest, HorizontalVerticalArrowKeys) {
  std::unique_ptr<HorizontalTestContainer> horizontal_container =
      std::make_unique<HorizontalTestContainer>(4);
  std::unique_ptr<VerticalTestContainer> vertical_container =
      std::make_unique<VerticalTestContainer>(4);

  containers_.clear();
  containers_.emplace_back(horizontal_container.get());
  containers_.emplace_back(vertical_container.get());

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(false);
}

TEST_F(ResultSelectionTest, HorizontalVerticalTab) {
  std::unique_ptr<HorizontalTestContainer> horizontal_container =
      std::make_unique<HorizontalTestContainer>(4);
  std::unique_ptr<VerticalTestContainer> vertical_container =
      std::make_unique<VerticalTestContainer>(4);

  containers_.clear();
  containers_.emplace_back(horizontal_container.get());
  containers_.emplace_back(vertical_container.get());

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(true);
}

TEST_F(ResultSelectionTest, TestVerticalStackArrows) {
  std::vector<std::unique_ptr<SearchResultContainerView>> vertical_containers =
      CreateContainerVector(4, 4, false);
  SetContainers(vertical_containers);

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(false);
}

TEST_F(ResultSelectionTest, TestVerticalStackTab) {
  std::vector<std::unique_ptr<SearchResultContainerView>> vertical_containers =
      CreateContainerVector(4, 4, false);
  SetContainers(vertical_containers);

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(true);
}

TEST_P(ResultSelectionTest, TestHorizontalStackArrows) {
  std::vector<std::unique_ptr<SearchResultContainerView>>
      horizontal_containers = CreateContainerVector(4, 4, true);
  SetContainers(horizontal_containers);

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(false);
}

TEST_F(ResultSelectionTest, TestHorizontalStackTab) {
  std::vector<std::unique_ptr<SearchResultContainerView>>
      horizontal_containers = CreateContainerVector(4, 4, true);
  SetContainers(horizontal_containers);

  // Initialize the RSC for test.
  result_selection_controller_->ResetSelection();

  TestMultiAxisTraversal(true);
}

}  // namespace app_list
