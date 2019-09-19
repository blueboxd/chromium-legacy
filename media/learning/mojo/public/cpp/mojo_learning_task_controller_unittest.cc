// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class MojoLearningTaskControllerTest : public ::testing::Test {
 public:
  // Impl of a mojom::LearningTaskController that remembers call arguments.
  class FakeMojoLearningTaskController : public mojom::LearningTaskController {
   public:
    void BeginObservation(const base::UnguessableToken& id,
                          const FeatureVector& features) override {
      begin_args_.id_ = id;
      begin_args_.features_ = features;
    }

    void CompleteObservation(const base::UnguessableToken& id,
                             const ObservationCompletion& completion) override {
      complete_args_.id_ = id;
      complete_args_.completion_ = completion;
    }

    void CancelObservation(const base::UnguessableToken& id) override {
      cancel_args_.id_ = id;
    }

    struct {
      base::UnguessableToken id_;
      FeatureVector features_;
    } begin_args_;

    struct {
      base::UnguessableToken id_;
      ObservationCompletion completion_;
    } complete_args_;

    struct {
      base::UnguessableToken id_;
    } cancel_args_;
  };

 public:
  MojoLearningTaskControllerTest()
      : learning_controller_binding_(&fake_learning_controller_) {}
  ~MojoLearningTaskControllerTest() override = default;

  void SetUp() override {
    // Create a LearningTask.
    task_.name = "MyLearningTask";

    // Create a fake learner provider mojo impl.
    mojom::LearningTaskControllerPtr learning_controller_ptr;
    learning_controller_binding_.Bind(
        mojo::MakeRequest(&learning_controller_ptr));

    // Tell |learning_controller_| to forward to the fake learner impl.
    learning_controller_ = std::make_unique<MojoLearningTaskController>(
        task_, std::move(learning_controller_ptr));
  }

  // Mojo stuff.
  base::test::TaskEnvironment task_environment_;

  LearningTask task_;
  FakeMojoLearningTaskController fake_learning_controller_;
  mojo::Binding<mojom::LearningTaskController> learning_controller_binding_;

  // The learner under test.
  std::unique_ptr<MojoLearningTaskController> learning_controller_;
};

TEST_F(MojoLearningTaskControllerTest, GetLearningTask) {
  EXPECT_EQ(learning_controller_->GetLearningTask().name, task_.name);
}

TEST_F(MojoLearningTaskControllerTest, Begin) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  learning_controller_->BeginObservation(id, features);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.begin_args_.id_);
  EXPECT_EQ(features, fake_learning_controller_.begin_args_.features_);
}

TEST_F(MojoLearningTaskControllerTest, Complete) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  ObservationCompletion completion(TargetValue(1234));
  learning_controller_->CompleteObservation(id, completion);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.complete_args_.id_);
  EXPECT_EQ(completion.target_value,
            fake_learning_controller_.complete_args_.completion_.target_value);
}

TEST_F(MojoLearningTaskControllerTest, Cancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  learning_controller_->CancelObservation(id);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.cancel_args_.id_);
}

}  // namespace learning
}  // namespace media
