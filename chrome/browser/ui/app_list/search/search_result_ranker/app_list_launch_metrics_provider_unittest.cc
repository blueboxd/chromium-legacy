// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_metrics_provider.h"

#include <set>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_state.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace app_list {

namespace {

using LaunchType = ::metrics::ChromeOSAppListLaunchEventProto::LaunchType;

using ::chromeos::ProfileHelper;
using ::metrics::ChromeOSAppListLaunchEventProto;

// 32 bytes long, matching the size of a real secret.
constexpr char kSecret[] = "abcdefghijklmnopqrstuvwxyzabcdef";

constexpr uint64_t kUserId = 1123u;

constexpr char kValue1[] = "value one";
constexpr char kValue2[] = "value two";
constexpr char kValue3[] = "value three";
constexpr char kValue4[] = "value four";

// These are hex encoded values of the first 8 bytes of sha256(kSecret +
// kValueN), generated with the sha256sum command. These are hex encoded to make
// debugging incorrect values easier.
constexpr char kValue1Hash[] = "E79C24CD2117A2BB";
constexpr char kValue2Hash[] = "506ECDDC0BA3C341";
constexpr char kValue3Hash[] = "1E0CDC361557A12F";
constexpr char kValue4Hash[] = "4875030730DE902A";

std::string HashToHex(uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

void ExpectLoggingEventEquals(ChromeOSAppListLaunchEventProto proto,
                              const char* target_hash,
                              const char* query_hash,
                              const char* domain_hash,
                              const char* app_hash,
                              int search_query_length) {
  EXPECT_EQ(ChromeOSAppListLaunchEventProto::APP_TILES, proto.launch_type());
  // Hour field is untested.
  EXPECT_EQ(kUserId, proto.recurrence_ranker_user_id());
  EXPECT_EQ(search_query_length, proto.search_query_length());

  EXPECT_EQ(target_hash, HashToHex(proto.hashed_target()));
  EXPECT_EQ(query_hash, HashToHex(proto.hashed_query()));
  EXPECT_EQ(domain_hash, HashToHex(proto.hashed_domain()));
  EXPECT_EQ(app_hash, HashToHex(proto.hashed_app()));
}

}  // namespace

class AppListLaunchMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::Optional<base::FilePath> GetTempDir() { return temp_dir_.GetPath(); }

  void MakeProvider() {
    // Using WrapUnique to access a private constructor.
    provider_ = base::WrapUnique(new AppListLaunchMetricsProvider(
        base::BindRepeating(&AppListLaunchMetricsProviderTest::GetTempDir,
                            base::Unretained(this))));
    provider_->OnRecordingEnabled();
  }

  void InitProvider() {
    AddLog("", "", "", "");
    Wait();
  }

  void ExpectUninitialized() {
    EXPECT_EQ(AppListLaunchMetricsProvider::InitState::UNINITIALIZED,
              provider_->init_state_);
  }

  void ExpectInitStarted() {
    EXPECT_EQ(AppListLaunchMetricsProvider::InitState::INIT_STARTED,
              provider_->init_state_);
  }

  void ExpectEnabled() {
    EXPECT_EQ(AppListLaunchMetricsProvider::InitState::ENABLED,
              provider_->init_state_);
  }

  void ExpectDisabled() {
    EXPECT_EQ(AppListLaunchMetricsProvider::InitState::DISABLED,
              provider_->init_state_);
  }

  base::Optional<Secret> GetSecret() { return provider_->secret_; }

  std::string ReadSecret() {
    std::string proto_str;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);

      base::FilePath path = temp_dir_.GetPath().AppendASCII(
          AppListLaunchMetricsProvider::kStateProtoFilename);
      CHECK(base::ReadFileToString(path, &proto_str));
    }

    auto proto = std::make_unique<AppListLaunchRecorderStateProto>();
    CHECK(proto->ParseFromString(proto_str));
    return proto->secret();
  }

  uint64_t ReadUserId() {
    std::string proto_str;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);

      base::FilePath path = temp_dir_.GetPath().AppendASCII(
          AppListLaunchMetricsProvider::kStateProtoFilename);
      CHECK(base::ReadFileToString(path, &proto_str));
    }

    auto proto = std::make_unique<AppListLaunchRecorderStateProto>();
    CHECK(proto->ParseFromString(proto_str));
    return proto->recurrence_ranker_user_id();
  }

  void WriteStateProto(const std::string& secret) {
    AppListLaunchRecorderStateProto proto;
    proto.set_recurrence_ranker_user_id(kUserId);
    proto.set_secret(secret);

    std::string proto_str;
    CHECK(proto.SerializeToString(&proto_str));
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      CHECK(base::ImportantFileWriter::WriteFileAtomically(
          temp_dir_.GetPath().AppendASCII(
              AppListLaunchMetricsProvider::kStateProtoFilename),
          proto_str, "AppListLaunchMetricsProviderTest"));
    }

    Wait();
  }

  void DeleteStateProto() {
    DeleteFile(temp_dir_.GetPath().AppendASCII(
                   AppListLaunchMetricsProvider::kStateProtoFilename),
               false);
  }

  void AddLog(
      const std::string& target,
      const std::string& query,
      const std::string& domain,
      const std::string& app,
      LaunchType launch_type = ChromeOSAppListLaunchEventProto::APP_TILES) {
    provider_->OnAppListLaunch({launch_type, target, query, domain, app});
  }

  google::protobuf::RepeatedPtrField<ChromeOSAppListLaunchEventProto>
  GetLogs() {
    metrics::ChromeUserMetricsExtension uma_log;
    provider_->ProvideCurrentSessionData(&uma_log);
    return uma_log.chrome_os_app_list_launch_event();
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("Apps.AppListLaunchRecorderError", 0);
  }

  void Wait() { scoped_task_environment_.RunUntilIdle(); }

  base::HistogramTester histogram_tester_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<AppListLaunchMetricsProvider> provider_;
};

TEST_F(AppListLaunchMetricsProviderTest, ProvidesNothingWhenUninitialized) {
  MakeProvider();

  ExpectUninitialized();
  EXPECT_TRUE(GetLogs().empty());
  ExpectNoErrors();
}

TEST_F(AppListLaunchMetricsProviderTest, SucceedsGeneratingNewSecret) {
  MakeProvider();
  InitProvider();

  ExpectEnabled();
  // Because the secret is random, we settle for just checking its length.
  // In the object:
  EXPECT_EQ(32ul, GetSecret().value().value.size());
  // On disk:
  EXPECT_EQ(32ul, ReadSecret().size());

  EXPECT_EQ(GetSecret().value().value, ReadSecret());

  histogram_tester_.ExpectUniqueSample("Apps.AppListLaunchRecorderError",
                                       MetricsProviderError::kNoStateProto, 1);
}

TEST_F(AppListLaunchMetricsProviderTest, SucceedsLoadingExistingSecret) {
  WriteStateProto(kSecret);

  MakeProvider();
  InitProvider();

  ExpectEnabled();
  EXPECT_EQ(kSecret, GetSecret().value().value);
  ExpectNoErrors();
}

TEST_F(AppListLaunchMetricsProviderTest, DisableOnInvalidSecret) {
  WriteStateProto("wrong length");

  MakeProvider();
  InitProvider();

  ExpectDisabled();
  histogram_tester_.ExpectUniqueSample("Apps.AppListLaunchRecorderError",
                                       MetricsProviderError::kInvalidSecret, 1);
}

// Tests that a call to ProvideCurrentSessionData populates protos for each log,
// and that those protos contain the right values.
TEST_F(AppListLaunchMetricsProviderTest, CorrectHashedValues) {
  WriteStateProto(kSecret);
  MakeProvider();
  InitProvider();

  AddLog(kValue1, kValue2, kValue3, kValue4);
  AddLog(kValue2, kValue3, kValue4, kValue1);
  AddLog(kValue3, kValue4, kValue1, kValue2);

  const auto& events = GetLogs();
  // events[0] is a dummy log created in |InitProvider|. We don't test for its
  // contents below.
  ASSERT_EQ(events.size(), 4);

  ExpectLoggingEventEquals(events[1], kValue1Hash, kValue2Hash, kValue3Hash,
                           kValue4Hash, std::string(kValue2).size());
  ExpectLoggingEventEquals(events[2], kValue2Hash, kValue3Hash, kValue4Hash,
                           kValue1Hash, std::string(kValue3).size());
  ExpectLoggingEventEquals(events[3], kValue3Hash, kValue4Hash, kValue1Hash,
                           kValue2Hash, std::string(kValue4).size());
  ExpectNoErrors();
}

// Tests that the logs reported in one call to ProvideCurrentSessionData do no
// appear in the next.
TEST_F(AppListLaunchMetricsProviderTest, EventsNotDuplicated) {
  WriteStateProto(kSecret);
  MakeProvider();
  InitProvider();

  AddLog(kValue1, kValue2, kValue3, kValue4);
  auto events = GetLogs();
  ASSERT_EQ(events.size(), 2);
  ExpectLoggingEventEquals(events[1], kValue1Hash, kValue2Hash, kValue3Hash,
                           kValue4Hash, std::string(kValue2).size());

  AddLog(kValue2, kValue3, kValue4, kValue1);
  events = GetLogs();
  ASSERT_EQ(events.size(), 1);
  ExpectLoggingEventEquals(events[0], kValue2Hash, kValue3Hash, kValue4Hash,
                           kValue1Hash, std::string(kValue3).size());

  EXPECT_TRUE(GetLogs().empty());
  ExpectNoErrors();
}

// Tests that logging events are dropped after an unreasonably large number of
// them are made between uploads.
TEST_F(AppListLaunchMetricsProviderTest, EventsAreCapped) {
  MakeProvider();
  InitProvider();

  const int max_events = AppListLaunchMetricsProvider::kMaxEventsPerUpload;

  // Not enough events to hit the cap.
  for (int i = 0; i < max_events / 2; ++i)
    AddLog(kValue1, kValue2, kValue3, kValue4);
  // One event from init, kMaxEventsPerUpload/2 from the loop.
  EXPECT_EQ(1 + max_events / 2, GetLogs().size());

  // Enough events to hit the cap.
  for (int i = 0; i < 2 * max_events; ++i)
    AddLog(kValue1, kValue2, kValue3, kValue4);
  EXPECT_EQ(max_events, GetLogs().size());

  histogram_tester_.ExpectBucketCount(
      "Apps.AppListLaunchRecorderError",
      MetricsProviderError::kMaxEventsPerUploadExceeded, 1);
}

// Tests that logging events that occur before the provider is initialized are
// still correctly logged after initialization.
TEST_F(AppListLaunchMetricsProviderTest,
       LaunchEventsBeforeInitializationAreRecorded) {
  WriteStateProto(kSecret);
  MakeProvider();

  // To begin with, the provider is uninitialized and has no logs at all.
  EXPECT_TRUE(GetLogs().empty());
  // These logs are added before the provider has finished initialising.
  AddLog(kValue1, kValue2, kValue3, kValue4);
  AddLog(kValue2, kValue3, kValue4, kValue1);
  AddLog(kValue3, kValue4, kValue1, kValue2);
  // Initialisation is finished here.
  Wait();

  const auto& events = GetLogs();
  ASSERT_EQ(events.size(), 3);
  ExpectLoggingEventEquals(events[0], kValue1Hash, kValue2Hash, kValue3Hash,
                           kValue4Hash, std::string(kValue2).size());
  ExpectLoggingEventEquals(events[1], kValue2Hash, kValue3Hash, kValue4Hash,
                           kValue1Hash, std::string(kValue3).size());
  ExpectLoggingEventEquals(events[2], kValue3Hash, kValue4Hash, kValue1Hash,
                           kValue2Hash, std::string(kValue4).size());

  EXPECT_TRUE(GetLogs().empty());
  ExpectNoErrors();
}

// Without an existing saved state, instantiating a metrics provider should save
// an almost certainly unique user ID and secret. Test this by creating a few
// blank-slate metrics providers.
TEST_F(AppListLaunchMetricsProviderTest, UserIDsAndSecretsAreUnique) {
  const int num_runs = 10;

  std::set<uint64_t> user_ids;
  std::set<std::string> secrets;
  for (int i = 0; i < num_runs; ++i) {
    DeleteStateProto();
    MakeProvider();
    InitProvider();
    secrets.insert(ReadSecret());
    user_ids.insert(ReadUserId());
  }

  EXPECT_EQ(static_cast<size_t>(num_runs), secrets.size());
  EXPECT_EQ(static_cast<size_t>(num_runs), user_ids.size());
}

}  // namespace app_list
