// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/metric_collector.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Name prefix of the histogram that represents the success and various failure
// modes for a collector.
const char kCollectionOutcomeHistogramPrefix[] = "ChromeOS.CWP.Collect";

// Name prefix of the histogram that counts the number of reports uploaded by a
// collector.
const char kUploadCountHistogramPrefix[] = "ChromeOS.CWP.Upload";

// An upper bound on the count of reports expected to be uploaded by an UMA
// callback.
const int kMaxValueUploadReports = 10;

// This is used to space out session restore collections in the face of several
// notifications in a short period of time. There should be no less than this
// much time between collections.
const int kMinIntervalBetweenSessionRestoreCollectionsInSec = 30;

// Returns a random TimeDelta uniformly selected between zero and |max|.
base::TimeDelta RandomTimeDelta(base::TimeDelta max) {
  if (max.is_zero())
    return max;
  return base::TimeDelta::FromMicroseconds(
      base::RandGenerator(max.InMicroseconds()));
}

// PerfDataProto is defined elsewhere with more fields than the definition in
// Chromium's copy of perf_data.proto. During deserialization, the protobuf
// data could contain fields that are defined elsewhere but not in
// perf_data.proto, resulting in some data in |unknown_fields| for the message
// types within PerfDataProto.
//
// This function deletes those dangling unknown fields if they are in messages
// containing strings. See comments in perf_data.proto describing the fields
// that have been intentionally left out. Note that all unknown fields will be
// removed from those messages, not just unknown string fields.
void RemoveUnknownFieldsFromMessagesWithStrings(PerfDataProto* proto) {
  // Clean up PerfEvent::MMapEvent and PerfEvent::CommEvent.
  for (PerfDataProto::PerfEvent& event : *proto->mutable_events()) {
    if (event.has_comm_event())
      event.mutable_comm_event()->mutable_unknown_fields()->clear();
    if (event.has_mmap_event())
      event.mutable_mmap_event()->mutable_unknown_fields()->clear();
  }
  // Clean up PerfBuildID.
  for (PerfDataProto::PerfBuildID& build_id : *proto->mutable_build_ids()) {
    build_id.mutable_unknown_fields()->clear();
  }
  // Clean up StringMetadata and StringMetadata::StringAndMd5sumPrefix.
  if (proto->has_string_metadata()) {
    proto->mutable_string_metadata()->mutable_unknown_fields()->clear();
    if (proto->string_metadata().has_perf_command_line_whole()) {
      proto->mutable_string_metadata()
          ->mutable_perf_command_line_whole()
          ->mutable_unknown_fields()
          ->clear();
    }
  }
}

}  // namespace

MetricCollector::MetricCollector(const std::string& name)
    : MetricCollector(name, CollectionParams()) {}

MetricCollector::MetricCollector(const std::string& name,
                                 const CollectionParams& collection_params)
    : collection_params_(collection_params) {
  collect_uma_histogram_ =
      std::string(kCollectionOutcomeHistogramPrefix) + name;
  upload_uma_histogram_ = std::string(kUploadCountHistogramPrefix) + name;
}

MetricCollector::~MetricCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MetricCollector::AddToUmaHistogram(CollectionAttemptStatus outcome) const {
  base::UmaHistogramEnumeration(collect_uma_histogram_, outcome,
                                CollectionAttemptStatus::NUM_OUTCOMES);
}

bool MetricCollector::GetSampledProfiles(
    std::vector<SampledProfile>* sampled_profiles) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ShouldUpload() || cached_profile_data_.empty()) {
    base::UmaHistogramExactLinear(upload_uma_histogram_, 0,
                                  kMaxValueUploadReports);
    return false;
  }

  base::UmaHistogramExactLinear(upload_uma_histogram_,
                                cached_profile_data_.size(),
                                kMaxValueUploadReports);
  sampled_profiles->insert(
      sampled_profiles->end(),
      std::make_move_iterator(cached_profile_data_.begin()),
      std::make_move_iterator(cached_profile_data_.end()));
  cached_profile_data_.clear();
  return true;
}

bool MetricCollector::ShouldUpload() const {
  return true;
}

void MetricCollector::SuspendDone(base::TimeDelta sleep_duration) {
  // Collect a profile only 1/|sampling_factor| of the time, to avoid
  // collecting too much data. (0 means disable the trigger)
  const auto& resume_params = collection_params_.resume_from_suspend;
  if (resume_params.sampling_factor == 0 ||
      base::RandGenerator(resume_params.sampling_factor) != 0)
    return;

  // Override any existing profiling.
  if (timer_.IsRunning())
    timer_.Stop();

  // Randomly pick a delay before doing the collection.
  base::TimeDelta collection_delay =
      RandomTimeDelta(resume_params.max_collection_delay);
  timer_.Start(FROM_HERE, collection_delay,
               base::BindOnce(&MetricCollector::CollectPerfDataAfterResume,
                              GetWeakPtr(), sleep_duration, collection_delay));
}

void MetricCollector::CollectPerfDataAfterResume(
    base::TimeDelta sleep_duration,
    base::TimeDelta time_after_resume) {
  // Fill out a SampledProfile protobuf that will contain the collected data.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(sleep_duration.InMilliseconds());
  sampled_profile->set_ms_after_resume(time_after_resume.InMilliseconds());

  CollectIfNecessary(std::move(sampled_profile));
}

void MetricCollector::OnSessionRestoreDone(int num_tabs_restored) {
  // Collect a profile only 1/|sampling_factor| of the time, to
  // avoid collecting too much data and potentially causing UI latency.
  // (0 means disable the trigger)
  const auto& restore_params = collection_params_.restore_session;
  if (restore_params.sampling_factor == 0 ||
      base::RandGenerator(restore_params.sampling_factor) != 0) {
    return;
  }

  const auto min_interval = base::TimeDelta::FromSeconds(
      kMinIntervalBetweenSessionRestoreCollectionsInSec);
  const base::TimeDelta time_since_last_collection =
      (base::TimeTicks::Now() - last_session_restore_collection_time_);
  // Do not collect if there hasn't been enough elapsed time since the last
  // collection.
  if (!last_session_restore_collection_time_.is_null() &&
      time_since_last_collection < min_interval) {
    return;
  }

  // Stop any existing scheduled collection.
  if (timer_.IsRunning())
    timer_.Stop();

  // Randomly pick a delay before doing the collection.
  base::TimeDelta collection_delay =
      RandomTimeDelta(restore_params.max_collection_delay);
  timer_.Start(
      FROM_HERE, collection_delay,
      base::BindOnce(&MetricCollector::CollectPerfDataAfterSessionRestore,
                     GetWeakPtr(), collection_delay, num_tabs_restored));
}

void MetricCollector::CollectPerfDataAfterSessionRestore(
    base::TimeDelta time_after_restore,
    int num_tabs_restored) {
  // Fill out a SampledProfile protobuf that will contain the collected data.
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::RESTORE_SESSION);
  sampled_profile->set_ms_after_restore(time_after_restore.InMilliseconds());
  sampled_profile->set_num_tabs_restored(num_tabs_restored);

  CollectIfNecessary(std::move(sampled_profile));
  last_session_restore_collection_time_ = base::TimeTicks::Now();
}

void MetricCollector::ScheduleIntervalCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();

  base::TimeTicks interval_end =
      next_profiling_interval_start_ + collection_params_.periodic_interval;
  if (now > interval_end) {
    // We somehow missed at least one window. Start over.
    next_profiling_interval_start_ = now;
    interval_end = now + collection_params_.periodic_interval;
  }

  // Pick a random time in the current interval.
  base::TimeTicks scheduled_time =
      next_profiling_interval_start_ +
      RandomTimeDelta(collection_params_.periodic_interval);
  // If the scheduled time has already passed in the time it took to make the
  // above calculations, trigger the collection event immediately.
  if (scheduled_time < now)
    scheduled_time = now;

  timer_.Start(
      FROM_HERE, scheduled_time - now,
      base::BindOnce(&MetricCollector::DoPeriodicCollection, GetWeakPtr()));

  // Update the profiling interval tracker to the start of the next interval.
  next_profiling_interval_start_ = interval_end;
}

void MetricCollector::DoPeriodicCollection() {
  auto sampled_profile = std::make_unique<SampledProfile>();
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  CollectIfNecessary(std::move(sampled_profile));
}

void MetricCollector::CollectIfNecessary(
    std::unique_ptr<SampledProfile> sampled_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldCollect()) {
    // Do the actual profile collection.
    CollectProfile(std::move(sampled_profile));
  }

  // Schedule another interval collection. This call makes sense regardless of
  // whether or not the current collection was interval-triggered. If it had
  // been another type of trigger event, the interval timer would have been
  // halted, so it makes sense to reschedule a new interval collection.
  ScheduleIntervalCollection();
}

bool MetricCollector::ShouldCollect() const {
  return true;
}

void MetricCollector::OnUserLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks now = base::TimeTicks::Now();
  login_time_ = now;
  next_profiling_interval_start_ = now;
  ScheduleIntervalCollection();
}

void MetricCollector::Deactivate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop the timer, but leave |cached_profile_data_| intact.
  timer_.Stop();
}

void MetricCollector::SaveSerializedPerfProto(
    std::unique_ptr<SampledProfile> sampled_profile,
    PerfProtoType type,
    const std::string& serialized_proto) {
  if (serialized_proto.empty()) {
    AddToUmaHistogram(CollectionAttemptStatus::ILLEGAL_DATA_RETURNED);
    return;
  }

  switch (type) {
    case PerfProtoType::PERF_TYPE_DATA: {
      PerfDataProto perf_data_proto;
      if (!perf_data_proto.ParseFromString(serialized_proto)) {
        AddToUmaHistogram(CollectionAttemptStatus::PROTOBUF_NOT_PARSED);
        return;
      }
      RemoveUnknownFieldsFromMessagesWithStrings(&perf_data_proto);
      sampled_profile->mutable_perf_data()->Swap(&perf_data_proto);
      break;
    }
    case PerfProtoType::PERF_TYPE_STAT: {
      PerfStatProto perf_stat_proto;
      if (!perf_stat_proto.ParseFromString(serialized_proto)) {
        AddToUmaHistogram(CollectionAttemptStatus::PROTOBUF_NOT_PARSED);
        return;
      }
      sampled_profile->mutable_perf_stat()->Swap(&perf_stat_proto);
      break;
    }
    case PerfProtoType::PERF_TYPE_UNSUPPORTED:
      AddToUmaHistogram(CollectionAttemptStatus::PROTOBUF_NOT_PARSED);
      return;
  }

  sampled_profile->set_ms_after_boot(base::SysInfo::Uptime().InMilliseconds());
  DCHECK(!login_time_.is_null());
  sampled_profile->set_ms_after_login(
      (base::TimeTicks::Now() - login_time_).InMilliseconds());

  // Add the collected data to the container of collected SampledProfiles.
  cached_profile_data_.resize(cached_profile_data_.size() + 1);
  cached_profile_data_.back().Swap(sampled_profile.get());
  AddToUmaHistogram(CollectionAttemptStatus::SUCCESS);
}

size_t MetricCollector::cached_profile_data_size() const {
  size_t data_size = 0;
  for (size_t i = 0; i < cached_profile_data_.size(); ++i) {
    data_size += cached_profile_data_[i].ByteSize();
  }
  return data_size;
}

}  // namespace metrics
