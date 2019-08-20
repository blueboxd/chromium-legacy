// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/learning_session_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningSessionImplTest : public testing::Test {
 public:
  class FakeLearningTaskController;
  using ControllerVector = std::vector<FakeLearningTaskController*>;
  using TaskRunnerVector = std::vector<base::SequencedTaskRunner*>;

  class FakeLearningTaskController : public LearningTaskController {
   public:
    // Send ControllerVector* as void*, else it complains that args can't be
    // forwarded.  Adding base::Unretained() doesn't help.
    FakeLearningTaskController(void* controllers,
                               const LearningTask& task,
                               SequenceBoundFeatureProvider feature_provider)
        : feature_provider_(std::move(feature_provider)) {
      static_cast<ControllerVector*>(controllers)->push_back(this);
      // As a complete hack, call the only public method on fp so that
      // we can verify that it was given to us by the session.
      if (!feature_provider_.is_null()) {
        feature_provider_.Post(FROM_HERE, &FeatureProvider::AddFeatures,
                               FeatureVector(),
                               FeatureProvider::FeatureVectorCB());
      }
    }

    void BeginObservation(base::UnguessableToken id,
                          const FeatureVector& features) override {
      id_ = id;
      features_ = features;
    }

    void CompleteObservation(base::UnguessableToken id,
                             const ObservationCompletion& completion) override {
      EXPECT_EQ(id_, id);
      example_.features = std::move(features_);
      example_.target_value = completion.target_value;
      example_.weight = completion.weight;
    }

    void CancelObservation(base::UnguessableToken id) override {
      cancelled_id_ = id;
    }

    SequenceBoundFeatureProvider feature_provider_;
    base::UnguessableToken id_;
    FeatureVector features_;
    LabelledExample example_;

    // Most recently cancelled id.
    base::UnguessableToken cancelled_id_;
  };

  class FakeFeatureProvider : public FeatureProvider {
   public:
    FakeFeatureProvider(bool* flag_ptr) : flag_ptr_(flag_ptr) {}

    // Do nothing, except note that we were called.
    void AddFeatures(FeatureVector features,
                     FeatureProvider::FeatureVectorCB cb) override {
      *flag_ptr_ = true;
    }

    bool* flag_ptr_ = nullptr;
  };

  LearningSessionImplTest() {
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
    session_ = std::make_unique<LearningSessionImpl>(task_runner_);
    session_->SetTaskControllerFactoryCBForTesting(base::BindRepeating(
        [](ControllerVector* controllers, TaskRunnerVector* task_runners,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           const LearningTask& task,
           SequenceBoundFeatureProvider feature_provider)
            -> base::SequenceBound<LearningTaskController> {
          task_runners->push_back(task_runner.get());
          return base::SequenceBound<FakeLearningTaskController>(
              task_runner, static_cast<void*>(controllers), task,
              std::move(feature_provider));
        },
        &task_controllers_, &task_runners_));

    task_0_.name = "task_0";
    task_1_.name = "task_1";
  }

  ~LearningSessionImplTest() override {
    // To prevent a memory leak, reset the session.  This will post destruction
    // of other objects, so RunUntilIdle().
    session_.reset();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<LearningSessionImpl> session_;

  LearningTask task_0_;
  LearningTask task_1_;

  ControllerVector task_controllers_;
  TaskRunnerVector task_runners_;
};

TEST_F(LearningSessionImplTest, RegisteringTasksCreatesControllers) {
  EXPECT_EQ(task_controllers_.size(), 0u);
  EXPECT_EQ(task_runners_.size(), 0u);

  session_->RegisterTask(task_0_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 1u);
  EXPECT_EQ(task_runners_.size(), 1u);
  EXPECT_EQ(task_runners_[0], task_runner_.get());

  session_->RegisterTask(task_1_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 2u);
  EXPECT_EQ(task_runners_.size(), 2u);
  EXPECT_EQ(task_runners_[1], task_runner_.get());
}

TEST_F(LearningSessionImplTest, ExamplesAreForwardedToCorrectTask) {
  session_->RegisterTask(task_0_);
  session_->RegisterTask(task_1_);

  base::UnguessableToken id = base::UnguessableToken::Create();

  LabelledExample example_0({FeatureValue(123), FeatureValue(456)},
                            TargetValue(1234));
  std::unique_ptr<LearningTaskController> ltc_0 =
      session_->GetController(task_0_.name);
  ltc_0->BeginObservation(id, example_0.features);
  ltc_0->CompleteObservation(
      id, ObservationCompletion(example_0.target_value, example_0.weight));

  LabelledExample example_1({FeatureValue(321), FeatureValue(654)},
                            TargetValue(4321));

  std::unique_ptr<LearningTaskController> ltc_1 =
      session_->GetController(task_1_.name);
  ltc_1->BeginObservation(id, example_1.features);
  ltc_1->CompleteObservation(
      id, ObservationCompletion(example_1.target_value, example_1.weight));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->example_, example_0);
  EXPECT_EQ(task_controllers_[1]->example_, example_1);
}

TEST_F(LearningSessionImplTest, ControllerLifetimeScopedToSession) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);

  // Destroy the session.  |controller| should still be usable, though it won't
  // forward requests anymore.
  session_.reset();
  task_environment_.RunUntilIdle();

  // Should not crash.
  controller->BeginObservation(base::UnguessableToken::Create(),
                               FeatureVector());
}

TEST_F(LearningSessionImplTest, FeatureProviderIsForwarded) {
  // Verify that a FeatureProvider actually gets forwarded to the LTC.
  bool flag = false;
  session_->RegisterTask(
      task_0_, base::SequenceBound<FakeFeatureProvider>(task_runner_, &flag));
  task_environment_.RunUntilIdle();
  // Registering the task should create a FakeLearningTaskController, which will
  // call AddFeatures on the fake FeatureProvider.
  EXPECT_TRUE(flag);
}

TEST_F(LearningSessionImplTest, DestroyingControllerCancelsObservations) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  // Start an observation and verify that it starts.
  base::UnguessableToken id = base::UnguessableToken::Create();
  controller->BeginObservation(id, FeatureVector());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->id_, id);
  EXPECT_NE(task_controllers_[0]->cancelled_id_, id);

  // Should result in cancelling the observation.
  controller.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->cancelled_id_, id);
}

}  // namespace learning
}  // namespace media
