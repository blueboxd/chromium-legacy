// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_collector.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite_inl.h"

namespace metrics {

namespace {

// Returns an example PerfDataProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto() {
  PerfDataProto proto;
  proto.set_timestamp_sec(1435604013);  // Time since epoch in seconds.

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Returns an example PerfStatProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |result| is an output parameter that will contain the created protobuf.
PerfStatProto GetExamplePerfStatProto() {
  PerfStatProto proto;
  proto.set_command_line(
      "perf stat -a -e cycles -e instructions -e branches -- sleep 2");

  PerfStatProto_PerfStatLine* line1 = proto.add_line();
  line1->set_time_ms(1000);
  line1->set_count(2000);
  line1->set_event_name("cycles");

  PerfStatProto_PerfStatLine* line2 = proto.add_line();
  line2->set_time_ms(2000);
  line2->set_count(5678);
  line2->set_event_name("instructions");

  PerfStatProto_PerfStatLine* line3 = proto.add_line();
  line3->set_time_ms(3000);
  line3->set_count(9999);
  line3->set_event_name("branches");

  return proto;
}

// Creates a serialized data stream containing a string with a field tag number.
std::string SerializeStringFieldWithTag(int field, const std::string& value) {
  std::string result;
  google::protobuf::io::StringOutputStream string_stream(&result);
  google::protobuf::io::CodedOutputStream output(&string_stream);

  using google::protobuf::internal::WireFormatLite;
  WireFormatLite::WriteTag(field, WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                           &output);
  output.WriteVarint32(value.size());
  output.WriteString(value);

  return result;
}

// Allows access to some private methods for testing.
class TestMetricCollector : public MetricCollector {
 public:
  TestMetricCollector() : TestMetricCollector(CollectionParams()) {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : MetricCollector("UMA.CWP.TestData", collection_params),
        weak_factory_(this) {}

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto();
    SaveSerializedPerfProto(std::move(sampled_profile),
                            PerfProtoType::PERF_TYPE_DATA,
                            perf_data_proto.SerializeAsString());
  }

  base::WeakPtr<MetricCollector> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  using MetricCollector::collection_params;
  using MetricCollector::login_time;
  using MetricCollector::PerfProtoType;
  using MetricCollector::SaveSerializedPerfProto;
  using MetricCollector::ScheduleIntervalCollection;
  using MetricCollector::timer;

 private:
  std::vector<SampledProfile> stored_profiles_;

  base::WeakPtrFactory<TestMetricCollector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

}  // namespace

class MetricCollectorTest : public testing::Test {
 public:
  MetricCollectorTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_),
        perf_data_proto_(GetExamplePerfDataProto()),
        perf_stat_proto_(GetExamplePerfStatProto()) {}

  void SetUp() override {
    CollectionParams test_params;
    // Set the sampling factors for the triggers to 1, so we always trigger
    // collection.
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.restore_session.sampling_factor = 1;

    metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
    metric_collector_->Init();

    // MetricCollector requires the user to be logged in.
    metric_collector_->OnUserLoggedIn();
  }

  void TearDown() override { metric_collector_.reset(); }

 protected:
  std::unique_ptr<TestMetricCollector> metric_collector_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  // Store sample perf data/stat protobufs for testing.
  PerfDataProto perf_data_proto_;
  PerfStatProto perf_stat_proto_;

  DISALLOW_COPY_AND_ASSIGN(MetricCollectorTest);
};

TEST_F(MetricCollectorTest, CheckSetup) {
  EXPECT_GT(perf_data_proto_.ByteSize(), 0);
  EXPECT_GT(perf_stat_proto_.ByteSize(), 0);

  // Timer is active after user logs in.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());

  // There are no cached profiles at start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_collector_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(MetricCollectorTest, EnabledOnLogin) {
  metric_collector_->OnUserLoggedIn();
  EXPECT_TRUE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());
}

TEST_F(MetricCollectorTest, EmptyProtosAreNotSaved) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_DATA, std::string());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(metric_collector_->GetSampledProfiles(&stored_profiles));
}

TEST_F(MetricCollectorTest, PerfDataProto) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_DATA,
      perf_data_proto_.SerializeAsString());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_FALSE(profile.has_perf_stat());
  EXPECT_EQ(perf_data_proto_.SerializeAsString(),
            profile.perf_data().SerializeAsString());
}

TEST_F(MetricCollectorTest, PerfDataProto_UnknownFieldsDiscarded) {
  // First add some unknown fields to MMapEvent, CommEvent, PerfBuildID, and
  // StringAndMd5sumPrefix. The known field values don't have to make sense for
  // perf data. They are just padding to avoid having an otherwise empty proto.
  // The unknown field string contents don't have to make sense as serialized
  // data as the test is to discard them.

  // MMapEvent
  PerfDataProto_PerfEvent* event1 = perf_data_proto_.add_events();
  event1->mutable_header()->set_type(1);
  event1->mutable_mmap_event()->set_pid(1234);
  event1->mutable_mmap_event()->set_filename_md5_prefix(0xdeadbeef);
  // Missing field |MMapEvent::filename| has tag=6.
  *event1->mutable_mmap_event()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(6, "/opt/google/chrome/chrome");

  // CommEvent
  PerfDataProto_PerfEvent* event2 = perf_data_proto_.add_events();
  event2->mutable_header()->set_type(2);
  event2->mutable_comm_event()->set_pid(5678);
  event2->mutable_comm_event()->set_comm_md5_prefix(0x900df00d);
  // Missing field |CommEvent::comm| has tag=3.
  *event2->mutable_comm_event()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(3, "chrome");

  // PerfBuildID
  PerfDataProto_PerfBuildID* build_id = perf_data_proto_.add_build_ids();
  build_id->set_misc(3);
  build_id->set_pid(1337);
  build_id->set_filename_md5_prefix(0x9876543210);
  // Missing field |PerfBuildID::filename| has tag=4.
  *build_id->mutable_unknown_fields() =
      SerializeStringFieldWithTag(4, "/opt/google/chrome/chrome");

  // StringAndMd5sumPrefix
  PerfDataProto_StringMetadata* metadata =
      perf_data_proto_.mutable_string_metadata();
  metadata->mutable_perf_command_line_whole()->set_value_md5_prefix(
      0x123456789);
  // Missing field |StringAndMd5sumPrefix::value| has tag=1.
  *metadata->mutable_perf_command_line_whole()->mutable_unknown_fields() =
      SerializeStringFieldWithTag(1, "perf record -a -- sleep 1");

  // Serialize to string and make sure it can be deserialized.
  std::string perf_data_string = perf_data_proto_.SerializeAsString();
  PerfDataProto temp_proto;
  EXPECT_TRUE(temp_proto.ParseFromString(perf_data_string));

  // Now pass it to |metric_collector_|.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_DATA, perf_data_string);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_perf_data());

  // The serialized form should be different because the unknown fields have
  // have been removed.
  EXPECT_NE(perf_data_string, profile.perf_data().SerializeAsString());

  // Check contents of stored protobuf.
  const PerfDataProto& stored_proto = profile.perf_data();
  ASSERT_EQ(2, stored_proto.events_size());

  // MMapEvent
  const PerfDataProto_PerfEvent& stored_event1 = stored_proto.events(0);
  EXPECT_EQ(1U, stored_event1.header().type());
  EXPECT_EQ(1234U, stored_event1.mmap_event().pid());
  EXPECT_EQ(0xdeadbeef, stored_event1.mmap_event().filename_md5_prefix());
  EXPECT_EQ(0U, stored_event1.mmap_event().unknown_fields().size());

  // CommEvent
  const PerfDataProto_PerfEvent& stored_event2 = stored_proto.events(1);
  EXPECT_EQ(2U, stored_event2.header().type());
  EXPECT_EQ(5678U, stored_event2.comm_event().pid());
  EXPECT_EQ(0x900df00d, stored_event2.comm_event().comm_md5_prefix());
  EXPECT_EQ(0U, stored_event2.comm_event().unknown_fields().size());

  // PerfBuildID
  ASSERT_EQ(1, stored_proto.build_ids_size());
  const PerfDataProto_PerfBuildID& stored_build_id = stored_proto.build_ids(0);
  EXPECT_EQ(3U, stored_build_id.misc());
  EXPECT_EQ(1337U, stored_build_id.pid());
  EXPECT_EQ(0x9876543210U, stored_build_id.filename_md5_prefix());
  EXPECT_EQ(0U, stored_build_id.unknown_fields().size());

  // StringAndMd5sumPrefix
  const PerfDataProto_StringMetadata& stored_metadata =
      stored_proto.string_metadata();
  EXPECT_EQ(0x123456789U,
            stored_metadata.perf_command_line_whole().value_md5_prefix());
  EXPECT_EQ(0U,
            stored_metadata.perf_command_line_whole().unknown_fields().size());
}

TEST_F(MetricCollectorTest, PerfStatProto) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_STAT,
      perf_stat_proto_.SerializeAsString());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_boot());
  EXPECT_TRUE(profile.has_ms_after_login());

  EXPECT_FALSE(profile.has_perf_data());
  ASSERT_TRUE(profile.has_perf_stat());
  EXPECT_EQ(perf_stat_proto_.SerializeAsString(),
            profile.perf_stat().SerializeAsString());
}

// Change |sampled_profile| between calls to SaveSerializedPerfProto().
TEST_F(MetricCollectorTest, MultipleCalls) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_DATA,
      perf_data_proto_.SerializeAsString());

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESTORE_SESSION);
  sampled_profile->set_ms_after_restore(3000);
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_STAT,
      perf_stat_proto_.SerializeAsString());

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(60000);
  sampled_profile->set_ms_after_resume(1500);
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_DATA,
      perf_data_proto_.SerializeAsString());

  sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  metric_collector_->SaveSerializedPerfProto(
      std::move(sampled_profile),
      TestMetricCollector::PerfProtoType::PERF_TYPE_STAT,
      perf_stat_proto_.SerializeAsString());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(4U, stored_profiles.size());

  {
    const SampledProfile& profile = stored_profiles[0];
    EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    ASSERT_TRUE(profile.has_perf_data());
    EXPECT_FALSE(profile.has_perf_stat());
    EXPECT_EQ(perf_data_proto_.SerializeAsString(),
              profile.perf_data().SerializeAsString());
  }

  {
    const SampledProfile& profile = stored_profiles[1];
    EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    EXPECT_EQ(3000, profile.ms_after_restore());
    EXPECT_FALSE(profile.has_perf_data());
    ASSERT_TRUE(profile.has_perf_stat());
    EXPECT_EQ(perf_stat_proto_.SerializeAsString(),
              profile.perf_stat().SerializeAsString());
  }

  {
    const SampledProfile& profile = stored_profiles[2];
    EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    EXPECT_EQ(60000, profile.suspend_duration_ms());
    EXPECT_EQ(1500, profile.ms_after_resume());
    ASSERT_TRUE(profile.has_perf_data());
    EXPECT_FALSE(profile.has_perf_stat());
    EXPECT_EQ(perf_data_proto_.SerializeAsString(),
              profile.perf_data().SerializeAsString());
  }

  {
    const SampledProfile& profile = stored_profiles[3];
    EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
    EXPECT_TRUE(profile.has_ms_after_boot());
    EXPECT_TRUE(profile.has_ms_after_login());
    EXPECT_FALSE(profile.has_perf_data());
    ASSERT_TRUE(profile.has_perf_stat());
    EXPECT_EQ(perf_stat_proto_.SerializeAsString(),
              profile.perf_stat().SerializeAsString());
  }
}

TEST_F(MetricCollectorTest, Deactivate) {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  metric_collector_->CollectProfile(std::move(sampled_profile));

  metric_collector_->OnUserLoggedIn();
  EXPECT_TRUE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());

  // Timer is stopped by Deactivate(), but login time and cached profiles stay.
  metric_collector_->Deactivate();
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
  EXPECT_FALSE(metric_collector_->login_time().is_null());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
}

TEST_F(MetricCollectorTest, SuspendDone) {
  const auto kSuspendDuration = base::TimeDelta::FromMinutes(3);

  metric_collector_->SuspendDone(kSuspendDuration);

  // Timer is active after the SuspendDone call.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks. This will run all the tasks already queued, but not
  // any new tasks created while executing the existing pending tasks. This is
  // important, because our collectors always queue a new periodic collection
  // task after each collection, regardless of trigger. Thus, RunUntilIdle
  // would never terminate.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile.trigger_event());
  EXPECT_EQ(kSuspendDuration.InMilliseconds(), profile.suspend_duration_ms());
  EXPECT_TRUE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  // Run all new pending tasks. This will run a periodic collection that was
  // scheduled by the previous collection event.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile2 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());
}

TEST_F(MetricCollectorTest, OnSessionRestoreDone) {
  const int kRestoredTabs = 7;

  metric_collector_->OnSessionRestoreDone(kRestoredTabs);

  // Timer is active after the OnSessionRestoreDone call.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed for periodic collection after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile.trigger_event());
  EXPECT_EQ(kRestoredTabs, profile.num_tabs_restored());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  // A second SessionRestoreDone call is throttled.
  metric_collector_->OnSessionRestoreDone(1);
  // Run all new pending tasks. This will run a periodic collection, but not a
  // second session restore.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile2 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile2.trigger_event());

  // Rerun any new pending tasks. This run should include a new periodic
  // collection, but no session restore.
  task_runner_->RunPendingTasks();

  stored_profiles.clear();
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());
  const SampledProfile& profile3 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile3.trigger_event());
}

TEST_F(MetricCollectorTest, ScheduleIntervalCollection) {
  // Timer is active after login and a periodic collection is scheduled.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(metric_collector_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  // Timer is rearmed after each collection.
  EXPECT_TRUE(metric_collector_->timer().IsRunning());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_FALSE(profile.has_suspend_duration_ms());
  EXPECT_FALSE(profile.has_ms_after_resume());
  EXPECT_TRUE(profile.has_ms_after_login());
  EXPECT_TRUE(profile.has_ms_after_boot());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_FALSE(profile.has_perf_stat());
  EXPECT_EQ(perf_data_proto_.SerializeAsString(),
            profile.perf_data().SerializeAsString());
}

// Setting the sampling factors to zero should disable the triggers.
// Otherwise, it could cause a div-by-zero crash.
TEST_F(MetricCollectorTest, ZeroSamplingFactorDisablesTrigger) {
  // Define params with zero sampling factors.
  CollectionParams test_params;
  test_params.resume_from_suspend.sampling_factor = 0;
  test_params.restore_session.sampling_factor = 0;

  metric_collector_ = std::make_unique<TestMetricCollector>(test_params);
  metric_collector_->Init();

  // Cancel the background collection.
  metric_collector_->Deactivate();
  EXPECT_FALSE(metric_collector_->timer().IsRunning())
      << "Sanity: timer should not be running.";

  // Calling SuspendDone or OnSessionRestoreDone should not start the timer
  // that triggers collection.
  metric_collector_->SuspendDone(base::TimeDelta::FromMinutes(10));
  EXPECT_FALSE(metric_collector_->timer().IsRunning());

  metric_collector_->OnSessionRestoreDone(100);
  EXPECT_FALSE(metric_collector_->timer().IsRunning());
}

}  // namespace metrics
