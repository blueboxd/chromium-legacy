// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/experiment_helper.h"

#include <memory>

#include "media/learning/common/learning_task_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {
namespace learning {

class MockLearningTaskController : public LearningTaskController {
 public:
  MockLearningTaskController(const LearningTask& task) : task_(task) {}
  ~MockLearningTaskController() override = default;

  MOCK_METHOD2(BeginObservation,
               void(base::UnguessableToken id, const FeatureVector& features));
  MOCK_METHOD2(CompleteObservation,
               void(base::UnguessableToken id,
                    const ObservationCompletion& completion));
  MOCK_METHOD1(CancelObservation, void(base::UnguessableToken id));

  const LearningTask& GetLearningTask() { return task_; }

 private:
  LearningTask task_;
  DISALLOW_COPY_AND_ASSIGN(MockLearningTaskController);
};

class ExperimentHelperTest : public testing::Test {
 public:
  void SetUp() override {
    const std::string feature_name_1("feature 1");
    const FeatureValue feature_value_1("feature value 1");

    const std::string feature_name_2("feature 2");
    const FeatureValue feature_value_2("feature value 2");

    const std::string feature_name_3("feature 3");
    const FeatureValue feature_value_3("feature value 3");
    dict_.Add(feature_name_1, feature_value_1);
    dict_.Add(feature_name_2, feature_value_2);
    dict_.Add(feature_name_3, feature_value_3);

    task_.feature_descriptions.push_back({"some other feature"});
    task_.feature_descriptions.push_back({feature_name_3});
    task_.feature_descriptions.push_back({feature_name_1});

    std::unique_ptr<MockLearningTaskController> controller =
        std::make_unique<MockLearningTaskController>(task_);
    controller_raw_ = controller.get();

    helper_ = std::make_unique<ExperimentHelper>(std::move(controller));
  }

  LearningTask task_;
  MockLearningTaskController* controller_raw_ = nullptr;
  std::unique_ptr<ExperimentHelper> helper_;

  FeatureDictionary dict_;
};

TEST_F(ExperimentHelperTest, BeginComplete) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _));
  helper_->BeginObservation(dict_);
  TargetValue target(123);
  EXPECT_CALL(*controller_raw_,
              CompleteObservation(_, ObservationCompletion(target)))
      .Times(1);
  helper_->CompleteObservationIfNeeded(target);

  // Make sure that a second Complete doesn't send anything.
  testing::Mock::VerifyAndClear(controller_raw_);
  EXPECT_CALL(*controller_raw_,
              CompleteObservation(_, ObservationCompletion(target)))
      .Times(0);
  helper_->CompleteObservationIfNeeded(target);
}

TEST_F(ExperimentHelperTest, BeginCancel) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _));
  helper_->BeginObservation(dict_);
  EXPECT_CALL(*controller_raw_, CancelObservation(_));
  helper_->CancelObservationIfNeeded();
}

TEST_F(ExperimentHelperTest, CompleteWithoutBeginDoesNothing) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CompleteObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CancelObservation(_)).Times(0);
  helper_->CompleteObservationIfNeeded(TargetValue(123));
}

TEST_F(ExperimentHelperTest, CancelWithoutBeginDoesNothing) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CompleteObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CancelObservation(_)).Times(0);
  helper_->CancelObservationIfNeeded();
}

TEST_F(ExperimentHelperTest, DoesNothingWithoutController) {
  // Make sure that nothing crashes if there's no controller.
  ExperimentHelper helper(nullptr);

  // Begin / complete.
  helper_->BeginObservation(dict_);
  TargetValue target(123);
  helper_->CompleteObservationIfNeeded(target);

  // Begin / cancel.
  helper_->BeginObservation(dict_);
  helper_->CancelObservationIfNeeded();

  // Cancel without begin.
  helper_->CancelObservationIfNeeded();
}

}  // namespace learning
}  // namespace media
