// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Describes a hypothetical HID haptic gamepad with two 16-bit vibration
// magnitude fields.
constexpr uint8_t kReportId = 0x42;
constexpr size_t kReportLength = 5;
constexpr HidHapticGamepadBase::HapticReportData kHapticReportData = {
    0x1234, 0xabcd, kReportId, kReportLength, 1, 3, 16, 0, 0xffff};

// The expected "stop vibration" report bytes (zero vibration).
constexpr uint8_t kStopVibrationData[] = {kReportId, 0x00, 0x00, 0x00, 0x00};
// The expected "start vibration" report bytes for an effect with 100% intensity
// on the "strong" vibration channel and 50% intensity on the "weak" vibration
// channel.
constexpr uint8_t kStartVibrationData[] = {kReportId, 0xff, 0xff, 0xff, 0x7f};

// Use 1 ms for all non-zero effect durations. There is no reason to test longer
// delays as they will be skipped anyway.
constexpr double kDurationMillis = 1.0;
constexpr double kNonZeroStartDelayMillis = 1.0;
// Setting |start_delay| to zero can cause additional reports to be sent.
constexpr double kZeroStartDelayMillis = 0.0;
// Vibration magnitudes for the strong and weak channels of a typical
// dual-rumble vibration effect. kStartVibrationData describes a report with
// these magnitudes.
constexpr double kStrongMagnitude = 1.0;  // 100% intensity
constexpr double kWeakMagnitude = 0.5;    // 50% intensity
// Zero vibration magnitude. kStopVibrationData describes a report with zero
// vibration on both channels.
constexpr double kZeroMagnitude = 0.0;

constexpr base::TimeDelta kPendingTaskDuration =
    base::TimeDelta::FromMillisecondsD(kDurationMillis);

// An implementation of HidHapticGamepadBase that records its output reports.
class FakeHidHapticGamepad : public HidHapticGamepadBase {
 public:
  FakeHidHapticGamepad(const HidHapticGamepadBase::HapticReportData& data)
      : HidHapticGamepadBase(data) {}
  ~FakeHidHapticGamepad() override = default;

  size_t WriteOutputReport(void* report, size_t report_length) override {
    std::vector<uint8_t> report_bytes(report_length);
    const uint8_t* report_begin = reinterpret_cast<uint8_t*>(report);
    const uint8_t* report_end = report_begin + report_length;
    std::copy(report_begin, report_end, report_bytes.begin());
    output_reports_.push_back(std::move(report_bytes));
    return report_length;
  }

  std::vector<std::vector<uint8_t>> output_reports_;
};

// Main test fixture
class HidHapticGamepadBaseTest : public testing::Test {
 public:
  HidHapticGamepadBaseTest()
      : start_vibration_output_report_(kStartVibrationData,
                                       kStartVibrationData + kReportLength),
        stop_vibration_output_report_(kStopVibrationData,
                                      kStopVibrationData + kReportLength),
        first_callback_count_(0),
        second_callback_count_(0),
        first_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        second_callback_result_(
            mojom::GamepadHapticsResult::GamepadHapticsResultError),
        gamepad_(std::make_unique<FakeHidHapticGamepad>(kHapticReportData)) {}

  void TearDown() override { gamepad_->Shutdown(); }

  void PostPlayEffect(
      double start_delay,
      double strong_magnitude,
      double weak_magnitude,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback) {
    gamepad_->PlayEffect(
        mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
        mojom::GamepadEffectParameters::New(kDurationMillis, start_delay,
                                            strong_magnitude, weak_magnitude),
        std::move(callback), base::ThreadTaskRunnerHandle::Get());
  }

  void PostResetVibration(
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback) {
    gamepad_->ResetVibration(std::move(callback),
                             base::ThreadTaskRunnerHandle::Get());
  }

  // Callback for the first PlayEffect or ResetVibration call in a test.
  void FirstCallback(mojom::GamepadHapticsResult result) {
    first_callback_count_++;
    first_callback_result_ = result;
  }

  // Callback for the second PlayEffect or ResetVibration call in a test. Use
  // this when multiple callbacks may be received and the test should check the
  // result codes for each.
  void SecondCallback(mojom::GamepadHapticsResult result) {
    second_callback_count_++;
    second_callback_result_ = result;
  }

  const std::vector<uint8_t> start_vibration_output_report_;
  const std::vector<uint8_t> stop_vibration_output_report_;
  int first_callback_count_;
  int second_callback_count_;
  mojom::GamepadHapticsResult first_callback_result_;
  mojom::GamepadHapticsResult second_callback_result_;
  std::unique_ptr<FakeHidHapticGamepad> gamepad_;
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME};

  DISALLOW_COPY_AND_ASSIGN(HidHapticGamepadBaseTest);
};

TEST_F(HidHapticGamepadBaseTest, PlayEffectTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Run the queued task and start vibration.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, ResetVibrationTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostResetVibration(base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                    base::Unretained(this)));

  // Run the queued task and reset vibration.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, ZeroVibrationTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  PostPlayEffect(kZeroStartDelayMillis, kZeroMagnitude, kZeroMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Run the queued task.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, StartDelayTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);

  // Issue PlayEffect with non-zero |start_delay|.
  PostPlayEffect(kNonZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Stop vibration for the delay period.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Start vibration.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            first_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, ZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Start a second effect with zero |start_delay|. This should cause the first
  // effect to be preempted before it calls SetVibration.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                                base::Unretained(this)));

  // Execute the pending tasks.
  scoped_task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result
  // without issuing a report. The second effect has started vibration.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, NonZeroStartDelayPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt this one with another effect.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Start a second effect with non-zero |start_delay|. This should cause the
  // first effect to be preempted before it calls SetVibration.
  PostPlayEffect(kNonZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                                base::Unretained(this)));

  // Execute the pending tasks.
  scoped_task_environment_.RunUntilIdle();

  // The first effect should have already returned with a "preempted" result.
  // Because the second effect has a non-zero |start_delay| and is preempting
  // another effect, it will call SetZeroVibration to ensure no vibration
  // occurs during its |start_delay| period.
  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Start vibration.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);
  EXPECT_GT(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Finish the effect.
  scoped_task_environment_.FastForwardBy(kPendingTaskDuration);

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_,
                                   start_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(HidHapticGamepadBaseTest, ResetVibrationPreemptionTest) {
  EXPECT_TRUE(gamepad_->output_reports_.empty());
  EXPECT_EQ(0, first_callback_count_);
  EXPECT_EQ(0, second_callback_count_);

  // Start an ongoing effect. We'll preempt it with a reset.
  PostPlayEffect(kZeroStartDelayMillis, kStrongMagnitude, kWeakMagnitude,
                 base::BindOnce(&HidHapticGamepadBaseTest::FirstCallback,
                                base::Unretained(this)));

  // Reset vibration. This should cause the effect to be preempted before it
  // calls SetVibration.
  PostResetVibration(base::BindOnce(&HidHapticGamepadBaseTest::SecondCallback,
                                    base::Unretained(this)));

  // Execute the pending tasks.
  scoped_task_environment_.RunUntilIdle();

  EXPECT_THAT(gamepad_->output_reports_,
              testing::ElementsAre(stop_vibration_output_report_));
  EXPECT_EQ(1, first_callback_count_);
  EXPECT_EQ(1, second_callback_count_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultPreempted,
            first_callback_result_);
  EXPECT_EQ(mojom::GamepadHapticsResult::GamepadHapticsResultComplete,
            second_callback_result_);
  EXPECT_EQ(scoped_task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

}  // namespace

}  // namespace device
