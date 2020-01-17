// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/smoothness_helper.h"

#include "base/run_loop.h"
#include "media/blink/blink_platform_with_task_environment.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using learning::FeatureValue;
using learning::FeatureVector;
using learning::LearningTask;
using learning::LearningTaskController;
using learning::ObservationCompletion;
using learning::TargetValue;

using testing::_;
using testing::AnyNumber;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::ResultOf;
using testing::Return;

// Helper for EXPECT_CALL argument matching on Optional<TargetValue>.  Applies
// matcher |m| to the TargetValue as a double.  For example:
// void Foo(base::Optional<TargetValue>);
// EXPECT_CALL(..., Foo(OPT_TARGET(Gt(0.9)))) will expect that the value of the
// Optional<TargetValue> passed to Foo() to be greather than 0.9 .
#define OPT_TARGET(m) \
  ResultOf([](const base::Optional<TargetValue>& v) { return (*v).value(); }, m)

// Same as above, but expects an ObservationCompletion.
#define COMPLETION_TARGET(m)                                                 \
  ResultOf(                                                                  \
      [](const ObservationCompletion& x) { return x.target_value.value(); }, \
      m)

class SmoothnessHelperTest : public testing::Test {
  class MockLearningTaskController : public LearningTaskController {
   public:
    MOCK_METHOD3(BeginObservation,
                 void(base::UnguessableToken id,
                      const FeatureVector& features,
                      const base::Optional<TargetValue>& default_target));

    MOCK_METHOD2(CompleteObservation,
                 void(base::UnguessableToken id,
                      const ObservationCompletion& completion));

    MOCK_METHOD1(CancelObservation, void(base::UnguessableToken id));

    MOCK_METHOD2(UpdateDefaultTarget,
                 void(base::UnguessableToken id,
                      const base::Optional<TargetValue>& default_target));

    MOCK_METHOD0(GetLearningTask, const LearningTask&());
  };

  class MockClient : public SmoothnessHelper::Client {
   public:
    ~MockClient() override = default;

    MOCK_CONST_METHOD0(DecodedFrameCount, unsigned(void));
    MOCK_CONST_METHOD0(DroppedFrameCount, unsigned(void));
  };

 public:
  void SetUp() override {
    auto consecutive_ltc = std::make_unique<MockLearningTaskController>();
    consecutive_ltc_ = consecutive_ltc.get();
    features_.push_back(FeatureValue(123));
    helper_ = SmoothnessHelper::Create(std::move(consecutive_ltc), features_,
                                       &client_);
    segment_size_ = SmoothnessHelper::SegmentSizeForTesting();
  }

  // Helper for EXPECT_CALL.
  base::Optional<TargetValue> Opt(double x) {
    return base::Optional<TargetValue>(TargetValue(x));
  }

  void FastForwardBy(base::TimeDelta amount) {
    BlinkPlatformWithTaskEnvironment::GetTaskEnvironment()->FastForwardBy(
        amount);
  }

  // Set the dropped / decoded totals that will be returned by the mock client.
  void SetFrameCounters(int dropped, int decoded) {
    ON_CALL(client_, DroppedFrameCount()).WillByDefault(Return(dropped));
    ON_CALL(client_, DecodedFrameCount()).WillByDefault(Return(decoded));
  }

  // Helper under test
  std::unique_ptr<SmoothnessHelper> helper_;

  MockLearningTaskController* consecutive_ltc_;

  MockClient client_;
  FeatureVector features_;

  base::TimeDelta segment_size_;
};

TEST_F(SmoothnessHelperTest, MaxBadWindowsRecordsTrue) {
  // Record three bad segments, and verify that it records 'true'.
  SetFrameCounters(0, 0);
  base::RunLoop().RunUntilIdle();
  int dropped_frames = 0;
  int total_frames = 0;

  // First segment has no dropped frames.  Should record 0.
  EXPECT_CALL(*consecutive_ltc_, BeginObservation(_, _, OPT_TARGET(0.0)))
      .Times(1);
  SetFrameCounters(dropped_frames += 0, total_frames += 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(consecutive_ltc_);

  // Second segment has a lot of dropped frames, so the target should increase.
  EXPECT_CALL(*consecutive_ltc_, UpdateDefaultTarget(_, OPT_TARGET(1.0)))
      .Times(1);
  SetFrameCounters(dropped_frames += 999, total_frames += 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(consecutive_ltc_);

  // Third segment looks nice, so nothing should update.
  EXPECT_CALL(*consecutive_ltc_, UpdateDefaultTarget(_, OPT_TARGET(_)))
      .Times(0);
  SetFrameCounters(dropped_frames += 0, total_frames += 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(consecutive_ltc_);

  // Fourth segment has dropped frames, but the default shouldn't change.
  // It's okay if it changes to the same value, but we just memorize that it
  // won't change at all.
  EXPECT_CALL(*consecutive_ltc_, UpdateDefaultTarget(_, OPT_TARGET(_)))
      .Times(0);
  SetFrameCounters(dropped_frames += 999, total_frames += 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(consecutive_ltc_);

  // The last segment is also bad, and should increase the max.
  EXPECT_CALL(*consecutive_ltc_, UpdateDefaultTarget(_, OPT_TARGET(2.0)))
      .Times(1);
  SetFrameCounters(dropped_frames += 999, total_frames += 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(consecutive_ltc_);
}

}  // namespace media
