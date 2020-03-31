// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_run_routine_job.h"

#include <limits>
#include <memory>

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// String constant identifying the id field in the result payload.
constexpr char kIdFieldName[] = "id";
// String constant identifying the status field in the result payload.
constexpr char kStatusFieldName[] = "status";

// String constant identifying the routine enum field in the command payload.
constexpr char kRoutineEnumFieldName[] = "routineEnum";
// String constant identifying the parameter dictionary field in the command
// payload.
constexpr char kParamsFieldName[] = "params";

// String constants identifying the parameter fields for the battery capacity
// routine.
constexpr char kLowMahFieldName[] = "lowMah";
constexpr char kHighMahFieldName[] = "highMah";

// String constants identifying the parameter fields for the battery health
// routine.
constexpr char kMaximumCycleCountFieldName[] = "maximumCycleCount";
constexpr char kPercentBatteryWearAllowedFieldName[] =
    "percentBatteryWearAllowed";

// String constants identifying the parameter fields for the routine.
constexpr char kLengthSecondsFieldName[] = "lengthSeconds";

// String constants identifying the parameter fields for the AC power routine.
constexpr char kExpectedStatusFieldName[] = "expectedStatus";
constexpr char kExpectedPowerTypeFieldName[] = "expectedPowerType";

// String constants identifying the parameter fields for the NVMe wear level
// routine.
constexpr char kWearLevelThresholdFieldName[] = "wearLevelThreshold";

// String constants identifying the parameter fields for the NVMe self-test
// routine.
constexpr char kNvmeSelfTestTypeFieldName[] = "nvmeSelfTestType";

// String constants identifying the parameter fields for the disk read routine
constexpr char kTypeFieldName[] = "type";
constexpr char kFileSizeMbFieldName[] = "fileSizeMb";

// String constants identifying the parameter fields for the prime search
// routine
constexpr char kMaxNumFieldName[] = "maxNum";

// String constants identifying the parameter field for the battery discharge
// routine.
constexpr char kMaximumDischargePercentAllowedFieldName[] =
    "maximumDischargePercentAllowed";

// Dummy values to populate cros_healthd's RunRoutineResponse.
constexpr uint32_t kId = 11;
constexpr chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum kStatus =
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning;

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 987123;

em::RemoteCommand GenerateCommandProto(
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeDelta age_of_command,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    base::Optional<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
        routine,
    base::Optional<base::Value> params) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_RUN_DIAGNOSTIC_ROUTINE);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  base::Value root_dict(base::Value::Type::DICTIONARY);
  if (routine.has_value()) {
    root_dict.SetIntKey(kRoutineEnumFieldName,
                        static_cast<int>(routine.value()));
  }
  if (params.has_value())
    root_dict.SetKey(kParamsFieldName, std::move(params).value());
  std::string payload;
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

std::string CreateSuccessPayload(
    uint32_t id,
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum status) {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(kIdFieldName, static_cast<int>(id));
  root_dict.SetIntKey(kStatusFieldName, static_cast<int>(status));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

std::string CreateInvalidParametersFailurePayload() {
  std::string payload;
  base::Value root_dict(base::Value::Type::DICTIONARY);
  root_dict.SetIntKey(
      kIdFieldName,
      static_cast<int>(chromeos::cros_healthd::mojom::kFailedToStartId));
  root_dict.SetIntKey(
      kStatusFieldName,
      static_cast<int>(chromeos::cros_healthd::mojom::
                           DiagnosticRoutineStatusEnum::kFailedToStart));
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

}  // namespace

class DeviceCommandRunRoutineJobTest : public testing::Test {
 protected:
  DeviceCommandRunRoutineJobTest();
  DeviceCommandRunRoutineJobTest(const DeviceCommandRunRoutineJobTest&) =
      delete;
  DeviceCommandRunRoutineJobTest& operator=(
      const DeviceCommandRunRoutineJobTest&) = delete;
  ~DeviceCommandRunRoutineJobTest() override;

  void InitializeJob(
      RemoteCommandJob* job,
      RemoteCommandJob::UniqueIDType unique_id,
      base::TimeTicks issued_time,
      base::TimeDelta idleness_cutoff,
      bool terminate_upon_input,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
      base::Value params);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::TimeTicks test_start_time_;
};

DeviceCommandRunRoutineJobTest::DeviceCommandRunRoutineJobTest() {
  chromeos::CrosHealthdClient::InitializeFake();
  test_start_time_ = base::TimeTicks::Now();
}

DeviceCommandRunRoutineJobTest::~DeviceCommandRunRoutineJobTest() {
  chromeos::CrosHealthdClient::Shutdown();

  // Wait for DeviceCommandRunRoutineJobTest to observe the
  // destruction of the client.
  base::RunLoop().RunUntilIdle();
}

void DeviceCommandRunRoutineJobTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    base::TimeDelta idleness_cutoff,
    bool terminate_upon_input,
    chromeos::cros_healthd::mojom::DiagnosticRoutineEnum routine,
    base::Value params) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(unique_id, base::TimeTicks::Now() - issued_time,
                           idleness_cutoff, terminate_upon_input, routine,
                           std::move(params)),
      nullptr));

  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

TEST_F(DeviceCommandRunRoutineJobTest, InvalidRoutineEnumInCommandPayload) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(
          kUniqueID, base::TimeTicks::Now() - test_start_time_,
          base::TimeDelta::FromSeconds(30),
          /*terminate_upon_input=*/false,
          static_cast<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>(
              std::numeric_limits<std::underlying_type<
                  chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>::type>::
                  max()),
          std::move(params_dict)),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

// Test that not specifying a routine causes the job initialization to fail.
TEST_F(DeviceCommandRunRoutineJobTest, CommandPayloadMissingRoutine) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(kUniqueID, base::TimeTicks::Now() - test_start_time_,
                           base::TimeDelta::FromSeconds(30),
                           /*terminate_upon_input=*/false,
                           /*routine=*/base::nullopt, std::move(params_dict)),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

// Test that not including a parameters dictionary causes the routine
// initialization to fail.
TEST_F(DeviceCommandRunRoutineJobTest, CommandPayloadMissingParamDict) {
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  EXPECT_FALSE(job->Init(
      base::TimeTicks::Now(),
      GenerateCommandProto(
          kUniqueID, base::TimeTicks::Now() - test_start_time_,
          base::TimeDelta::FromSeconds(30),
          /*terminate_upon_input=*/false,
          chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
          /*params=*/base::nullopt),
      nullptr));

  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryCapacityRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLowMahFieldName, /*low_mah=*/90812);
  params_dict.SetIntKey(kHighMahFieldName, /*high_mah=*/986909);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lowMah parameter causes the battery capacity
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryCapacityRoutineMissingLowMah) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kHighMahFieldName, /*high_mah=*/986909);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the highMah parameter causes the battery capacity
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryCapacityRoutineMissingHighMah) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLowMahFieldName, /*low_mah=*/90812);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative lowMah parameter causes the battery capacity routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryCapacityRoutineInvalidLowMah) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLowMahFieldName, /*low_mah=*/-1);
  params_dict.SetIntKey(kHighMahFieldName, /*high_mah=*/986909);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative highMah parameter causes the battery capacity routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryCapacityRoutineInvalidHighMah) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLowMahFieldName, /*low_mah=*/90812);
  params_dict.SetIntKey(kHighMahFieldName, /*high_mah=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryHealthRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumCycleCountFieldName,
                        /*maximum_cycle_count=*/12);
  params_dict.SetIntKey(kPercentBatteryWearAllowedFieldName,
                        /*percent_battery_wear_allowed=*/78);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the maximumCycleCount parameter causes the battery
// health routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryHealthRoutineMissingMaximumCycleCount) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kPercentBatteryWearAllowedFieldName,
                        /*percent_battery_wear_allowed=*/78);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the percentBatteryWearAllowed parameter causes the
// battery health routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryHealthRoutineMissingPercentBatteryWearAllowed) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumCycleCountFieldName,
                        /*maximum_cycle_count=*/12);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative maximumCycleCount parameter causes the battery health
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryHealthRoutineInvalidMaximumCycleCount) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumCycleCountFieldName,
                        /*maximum_cycle_count=*/-1);
  params_dict.SetIntKey(kPercentBatteryWearAllowedFieldName,
                        /*percent_battery_wear_allowed=*/78);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative percentBatteryWearAllowed parameter causes the battery
// health routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryHealthRoutineInvalidPercentBatteryWearAllowed) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumCycleCountFieldName,
                        /*maximum_cycle_count=*/12);
  params_dict.SetIntKey(kPercentBatteryWearAllowedFieldName,
                        /*percent_battery_wear_allowed=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the urandom routine
// to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative lengthSeconds parameter causes the urandom routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunUrandomRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Note that the smartctl check routine has no parameters, so we only need to
// test that it can be run successfully.
TEST_F(DeviceCommandRunRoutineJobTest, RunSmartctlCheckRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that the AC power routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kExpectedStatusFieldName,
      /*expected_status=*/static_cast<int>(
          chromeos::cros_healthd::mojom::AcPowerStatusEnum::kConnected));
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           /*expected_power_type=*/"power_type");
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that the AC power routine succeeds without the optional parameter
// expectedPowerType specified.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunAcPowerRoutineNoOptionalExpectedPowerType) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kExpectedStatusFieldName,
      /*expected_status=*/static_cast<int>(
          chromeos::cros_healthd::mojom::AcPowerStatusEnum::kConnected));
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the expectedStatus parameter causes the AC power
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineMissingExpectedStatus) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           /*expected_power_type=*/"power_type");
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the expectedStatus parameter causes the AC
// power routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunAcPowerRoutineInvalidExpectedStatus) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  auto expected_status =
      static_cast<chromeos::cros_healthd::mojom::AcPowerStatusEnum>(
          std::numeric_limits<std::underlying_type<
              chromeos::cros_healthd::mojom::AcPowerStatusEnum>::type>::max());
  params_dict.SetIntKey(kExpectedStatusFieldName,
                        static_cast<int>(expected_status));
  params_dict.SetStringKey(kExpectedPowerTypeFieldName,
                           /*expected_power_type=*/"power_type");
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kAcPower,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the CPU cache
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative lengthSeconds parameter causes the CPU cache routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunCpuCacheRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuCache,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunCpuStressRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the CPU stress
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunCpuStressRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative lengthSeconds parameter causes the CPU stress routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunCpuStressRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kCpuStress,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunFloatingPointAccuracyRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                    kFloatingPointAccuracy,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the floating point
// accuracy routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunFloatingPointAccuracyRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                    kFloatingPointAccuracy,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative lengthSeconds parameter causes the floating point
// accuracy routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunFloatingPointAccuracyRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::
                    kFloatingPointAccuracy,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunNvmeWearLevelRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kWearLevelThresholdFieldName,
                        /*wear_level_threshold=*/50);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the wearLevelThreshold parameter causes the NVMe wear
// level routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeWearLevelRoutineMissingWearLevelThreshold) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that a negative wearLevelThreshold parameter causes the NVMe wear level
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeWearLevelRoutineInvalidWearLevelThreshold) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kWearLevelThresholdFieldName,
                        /*wear_level_threshold=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeWearLevel,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunNvmeSelfTestRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kNvmeSelfTestTypeFieldName,
      /*nvme_self_test_type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum::kShortSelfTest));
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the nvmeSelfTestType parameter causes the NVMe self
// test routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeSelfTestRoutineMissingSelfTestType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the nvmeSelfTestType parameter causes the NVMe
// self test routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunNvmeSelfTestRoutineInvalidSelfTestType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  auto nvme_self_test_type = std::numeric_limits<std::underlying_type<
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum>::type>::max();
  params_dict.SetIntKey(kNvmeSelfTestTypeFieldName,
                        static_cast<int>(nvme_self_test_type));
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kNvmeSelfTest,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that the disk read routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kTypeFieldName,
      /*type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/512);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the type parameter causes the disk read routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/512);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kTypeFieldName,
      /*type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/512);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the fileSizeMb parameter causes the disk read routine
// to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineMissingFileSizeMb) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kTypeFieldName,
      /*type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the type parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidType) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  auto type =
      static_cast<chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum>(
          std::numeric_limits<std::underlying_type<
              chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum>::type>::
              max());
  params_dict.SetIntKey(kTypeFieldName, static_cast<int>(type));
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/512);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the lengthSeconds parameter causes the disk
// read routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kTypeFieldName,
      /*type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/512);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the fileSizeMb parameter causes the disk read
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunDiskReadRoutineInvalidFileSizeMb) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(
      kTypeFieldName,
      /*type=*/static_cast<int>(
          chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum::kLinearRead));
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kFileSizeMbFieldName,
                        /*file_size_mb=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(job.get(), kUniqueID, test_start_time_,
                base::TimeDelta::FromSeconds(30),
                /*terminate_upon_input=*/false,
                chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kDiskRead,
                std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that the prime search routine succeeds with all parameters specified.
TEST_F(DeviceCommandRunRoutineJobTest, RunPrimeSearchRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kMaxNumFieldName,
                        /*max_num=*/100000);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the lengthSeconds parameter causes the prime search
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunPrimeSearchRoutineMissingLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaxNumFieldName,
                        /*max_num=*/100000);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that leaving out the maxNum parameter causes the prime search routine to
// fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunPrimeSearchRoutineMissingMaxNum) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the lengthSeconds parameter causes the prime
// search routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest,
       RunPrimeSearchRoutineInvalidLengthSeconds) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/-1);
  params_dict.SetIntKey(kMaxNumFieldName,
                        /*max_num=*/100000);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

// Test that an invalid value for the maxNum parameter causes the prime search
// routine to fail.
TEST_F(DeviceCommandRunRoutineJobTest, RunPrimeSearchRoutineInvalidMaxNum) {
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName,
                        /*length_seconds=*/2342);
  params_dict.SetIntKey(kMaxNumFieldName,
                        /*max_num=*/-1);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kPrimeSearch,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest, RunBatteryDischargeRoutineSuccess) {
  auto run_routine_response =
      chromeos::cros_healthd::mojom::RunRoutineResponse::New(kId, kStatus);
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetRunRoutineResponseForTesting(run_routine_response);
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, /*length_seconds=*/10);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName,
                        /*maximum_discharge_percent_allowed=*/76);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::SUCCEEDED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateSuccessPayload(kId, kStatus), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineMissingLengthSeconds) {
  // Test that leaving out the lengthSeconds parameter causes the routine to
  // fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName,
                        /*maximum_discharge_percent_allowed=*/76);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineMissingMaximumDischargePercentAllowed) {
  // Test that leaving out the maximumDischargePercentAllowed parameter causes
  // the routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, /*length_seconds=*/10);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineInvalidLengthSeconds) {
  // Test that a negative lengthSeconds parameter causes the routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, /*length_seconds=*/-10);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName,
                        /*maximum_discharge_percent_allowed=*/76);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

TEST_F(DeviceCommandRunRoutineJobTest,
       RunBatteryDischargeRoutineInvalidMaximumDischargePercentAllowed) {
  // Test that a negative maximumDischargePercentAllowed parameter causes the
  // routine to fail.
  base::Value params_dict(base::Value::Type::DICTIONARY);
  params_dict.SetIntKey(kLengthSecondsFieldName, /*length_seconds=*/10);
  params_dict.SetIntKey(kMaximumDischargePercentAllowedFieldName,
                        /*maximum_discharge_percent_allowed=*/-76);
  std::unique_ptr<RemoteCommandJob> job =
      std::make_unique<DeviceCommandRunRoutineJob>();
  InitializeJob(
      job.get(), kUniqueID, test_start_time_, base::TimeDelta::FromSeconds(30),
      /*terminate_upon_input=*/false,
      chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryDischarge,
      std::move(params_dict));
  base::RunLoop run_loop;
  bool success =
      job->Run(base::Time::Now(), base::TimeTicks::Now(),
               base::BindLambdaForTesting([&]() {
                 EXPECT_EQ(job->status(), RemoteCommandJob::FAILED);
                 std::unique_ptr<std::string> payload = job->GetResultPayload();
                 EXPECT_TRUE(payload);
                 EXPECT_EQ(CreateInvalidParametersFailurePayload(), *payload);
                 run_loop.Quit();
               }));
  EXPECT_TRUE(success);
  run_loop.Run();
}

}  // namespace policy
