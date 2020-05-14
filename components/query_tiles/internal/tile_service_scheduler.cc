// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_scheduler.h"

#include <memory>

#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/internal/tile_config.h"
#include "net/base/backoff_entry_serializer.h"

namespace query_tiles {
namespace {

// Key for query tiles backoff entry stored in pref service.
constexpr char kBackoffEntryKey[] = "query_tiles.backoff_entry_key";

class TileServiceSchedulerImpl : public TileServiceScheduler {
 public:
  TileServiceSchedulerImpl(
      background_task::BackgroundTaskScheduler* scheduler,
      PrefService* prefs,
      base::Clock* clock,
      const base::TickClock* tick_clock,
      std::unique_ptr<net::BackoffEntry::Policy> backoff_policy)
      : scheduler_(scheduler),
        prefs_(prefs),
        clock_(clock),
        tick_clock_(tick_clock),
        backoff_policy_(std::move(backoff_policy)),
        is_suspend_(false) {}

 private:
  // TileServiceScheduler implementation.
  void CancelTask() override {
    scheduler_->Cancel(
        static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID));
    ResetBackoff();
  }

  void OnFetchCompleted(TileInfoRequestStatus status) override {
    if (status == TileInfoRequestStatus::kShouldSuspend) {
      MaximizeBackoff();
      ScheduleTask(false);
      is_suspend_ = true;
    } else if (status == TileInfoRequestStatus::kFailure) {
      AddBackoff();
      ScheduleTask(false);
    } else if (status == TileInfoRequestStatus::kSuccess && !is_suspend_) {
      ResetBackoff();
      ScheduleTask(true);
    }
  }

  void OnTileManagerInitialized(TileGroupStatus status) override {
    if (status == TileGroupStatus::kNoTiles && !is_suspend_) {
      ResetBackoff();
      ScheduleTask(true);
    } else if (status == TileGroupStatus::kFailureDbOperation) {
      MaximizeBackoff();
      ScheduleTask(false);
      is_suspend_ = true;
    }
  }

  void ScheduleTask(bool is_init_schedule) {
    background_task::OneOffInfo one_off_task_info;
    GetTaskWindow(&one_off_task_info.window_start_time_ms,
                  &one_off_task_info.window_end_time_ms, is_init_schedule);
    background_task::TaskInfo task_info(
        static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID),
        one_off_task_info);
    task_info.is_persisted = true;
    task_info.update_current = true;
    task_info.network_type =
        TileConfig::GetIsUnMeteredNetworkRequired()
            ? background_task::TaskInfo::NetworkType::UNMETERED
            : background_task::TaskInfo::NetworkType::ANY;
    scheduler_->Schedule(task_info);
  }

  void AddBackoff() {
    std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
    current->InformOfRequest(false);
    UpdateBackoff(current.get());
  }

  void ResetBackoff() {
    std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
    current->Reset();
    UpdateBackoff(current.get());
  }

  void MaximizeBackoff() {
    std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
    current->Reset();
    current->SetCustomReleaseTime(
        tick_clock_->NowTicks() +
        base::TimeDelta::FromMilliseconds(backoff_policy_->maximum_backoff_ms));
    UpdateBackoff(current.get());
  }

  int64_t GetDelaysFromBackoff() {
    return GetCurrentBackoff()->GetTimeUntilRelease().InMilliseconds();
  }

  void GetTaskWindow(int64_t* start_time_ms,
                     int64_t* end_time_ms,
                     bool is_init_schedule) {
    if (is_init_schedule) {
      *start_time_ms =
          clock_->Now().ToDeltaSinceWindowsEpoch().InMilliseconds() +
          TileConfig::GetScheduleIntervalInMs() +
          base::RandGenerator(TileConfig::GetMaxRandomWindowInMs());
    } else {
      *start_time_ms = GetDelaysFromBackoff();
    }
    *end_time_ms = *start_time_ms + TileConfig::GetOneoffTaskWindowInMs();
  }

  std::unique_ptr<net::BackoffEntry> GetCurrentBackoff() {
    std::unique_ptr<net::BackoffEntry> result;
    const base::ListValue* value = prefs_->GetList(kBackoffEntryKey);
    if (value) {
      result = net::BackoffEntrySerializer::DeserializeFromValue(
          *value, backoff_policy_.get(), tick_clock_, clock_->Now());
    }
    if (!result) {
      return std::make_unique<net::BackoffEntry>(backoff_policy_.get(),
                                                 tick_clock_);
    }
    return result;
  }

  void UpdateBackoff(net::BackoffEntry* backoff) {
    std::unique_ptr<base::Value> value =
        net::BackoffEntrySerializer::SerializeToValue(*backoff, clock_->Now());
    prefs_->Set(kBackoffEntryKey, *value);
  }

  // Native Background Scheduler instance.
  background_task::BackgroundTaskScheduler* scheduler_;

  // PrefService.
  PrefService* prefs_;

  // Clock object to get current time.
  base::Clock* clock_;

  // TickClock used for building backoff entry.
  const base::TickClock* tick_clock_;

  // Backoff policy used for reschdule.
  std::unique_ptr<net::BackoffEntry::Policy> backoff_policy_;

  // Flag to indicate if currently have a suspend status to avoid overwriting if
  // already scheduled a suspend task during this lifecycle.
  bool is_suspend_;
};

}  // namespace

std::unique_ptr<TileServiceScheduler> TileServiceScheduler::Create(
    background_task::BackgroundTaskScheduler* scheduler,
    PrefService* prefs,
    base::Clock* clock,
    const base::TickClock* tick_clock,
    std::unique_ptr<net::BackoffEntry::Policy> backoff_policy) {
  return std::make_unique<TileServiceSchedulerImpl>(
      scheduler, prefs, clock, tick_clock, std::move(backoff_policy));
}

TileServiceScheduler::TileServiceScheduler() = default;

TileServiceScheduler::~TileServiceScheduler() = default;

}  // namespace query_tiles
